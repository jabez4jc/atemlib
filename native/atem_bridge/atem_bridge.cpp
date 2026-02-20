#include "atem_bridge.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

#include <CoreFoundation/CoreFoundation.h>
#include <pthread.h>

#include "BMDSwitcherAPI.h"

struct atem_connection
{
    IBMDSwitcher* switcher = nullptr;
    IBMDSwitcherMediaPool* media_pool = nullptr;
    IBMDSwitcherStills* stills = nullptr;
};

namespace
{
    constexpr int32_t kErrorBufferMin = 1;
    constexpr int32_t kSuccess = 0;
    constexpr int32_t kInternalError = -1;
    constexpr int32_t kTimeoutError = -2;

    constexpr char kBMDSwitcherBundlePath[] = "/Library/Application Support/Blackmagic Design/Switchers/BMDSwitcherAPI.bundle";

    using CreateDiscoveryFn = IBMDSwitcherDiscovery* (*)();

    pthread_once_t g_init_once = PTHREAD_ONCE_INIT;
    CFBundleRef g_bundle_ref = nullptr;
    CreateDiscoveryFn g_create_discovery = nullptr;

    class UploadLockCallback final : public IBMDSwitcherLockCallback
    {
    public:
        UploadLockCallback() = default;

        HRESULT Obtained() override
        {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                obtained_ = true;
            }
            cv_.notify_all();
            return S_OK;
        }

        bool WaitForObtained(std::chrono::milliseconds timeout)
        {
            std::unique_lock<std::mutex> lock(mutex_);
            return cv_.wait_for(lock, timeout, [&]() { return obtained_; });
        }

        HRESULT QueryInterface(REFIID, LPVOID* ppv) override
        {
            if (ppv == nullptr)
            {
                return E_POINTER;
            }

            *ppv = this;
            AddRef();
            return S_OK;
        }

        ULONG AddRef() override
        {
            return ++ref_count_;
        }

        ULONG Release() override
        {
            ULONG value = --ref_count_;
            if (value == 0)
            {
                delete this;
            }
            return value;
        }

    private:
        std::atomic<ULONG> ref_count_{1};
        std::mutex mutex_;
        std::condition_variable cv_;
        bool obtained_ = false;
    };

    class UploadStillsCallback final : public IBMDSwitcherStillsCallback
    {
    public:
        UploadStillsCallback() = default;

        HRESULT Notify(BMDSwitcherMediaPoolEventType eventType, IBMDSwitcherFrame*, int32_t) override
        {
            if (eventType == bmdSwitcherMediaPoolEventTypeTransferCompleted)
            {
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    completed_ = true;
                }
                cv_.notify_all();
            }

            return S_OK;
        }

        bool WaitForCompleted(std::chrono::milliseconds timeout)
        {
            std::unique_lock<std::mutex> lock(mutex_);
            return cv_.wait_for(lock, timeout, [&]() { return completed_; });
        }

        HRESULT QueryInterface(REFIID, LPVOID* ppv) override
        {
            if (ppv == nullptr)
            {
                return E_POINTER;
            }

            *ppv = this;
            AddRef();
            return S_OK;
        }

        ULONG AddRef() override
        {
            return ++ref_count_;
        }

        ULONG Release() override
        {
            ULONG value = --ref_count_;
            if (value == 0)
            {
                delete this;
            }
            return value;
        }

    private:
        std::atomic<ULONG> ref_count_{1};
        std::mutex mutex_;
        std::condition_variable cv_;
        bool completed_ = false;
    };

    void SetError(char* error_buffer, int32_t error_buffer_len, const char* message)
    {
        if (error_buffer == nullptr || error_buffer_len < kErrorBufferMin)
        {
            return;
        }

        std::snprintf(error_buffer, static_cast<size_t>(error_buffer_len), "%s", message != nullptr ? message : "unknown error");
    }

    void SetErrorFromHResult(char* error_buffer, int32_t error_buffer_len, const char* action, HRESULT result)
    {
        if (error_buffer == nullptr || error_buffer_len < kErrorBufferMin)
        {
            return;
        }

        std::snprintf(error_buffer, static_cast<size_t>(error_buffer_len), "%s failed (HRESULT=0x%08X)", action, static_cast<unsigned int>(result));
    }

    std::string CFStringToUtf8(CFStringRef value)
    {
        if (value == nullptr)
        {
            return "";
        }

        CFIndex length = CFStringGetLength(value);
        CFIndex max_size = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
        std::string output(static_cast<size_t>(max_size), '\0');

        if (!CFStringGetCString(value, output.data(), max_size, kCFStringEncodingUTF8))
        {
            return "";
        }

        output.resize(std::strlen(output.c_str()));
        return output;
    }

    CFStringRef Utf8ToCFString(const char* value)
    {
        return CFStringCreateWithCString(kCFAllocatorDefault, value != nullptr ? value : "", kCFStringEncodingUTF8);
    }

    void InitSwitcherApi()
    {
        CFStringRef bundle_path = CFStringCreateWithCString(kCFAllocatorDefault, kBMDSwitcherBundlePath, kCFStringEncodingUTF8);
        if (bundle_path == nullptr)
        {
            return;
        }

        CFURLRef bundle_url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, bundle_path, kCFURLPOSIXPathStyle, true);
        CFRelease(bundle_path);
        if (bundle_url == nullptr)
        {
            return;
        }

        g_bundle_ref = CFBundleCreate(kCFAllocatorDefault, bundle_url);
        CFRelease(bundle_url);

        if (g_bundle_ref == nullptr)
        {
            return;
        }

        g_create_discovery = reinterpret_cast<CreateDiscoveryFn>(
            CFBundleGetFunctionPointerForName(g_bundle_ref, CFSTR("GetBMDSwitcherDiscoveryInstance_0012")));
    }

    IBMDSwitcherDiscovery* CreateDiscovery()
    {
        pthread_once(&g_init_once, InitSwitcherApi);
        if (g_create_discovery == nullptr)
        {
            return nullptr;
        }

        return g_create_discovery();
    }

    int32_t EnsureConnection(atem_connection* connection, char* error_buffer, int32_t error_buffer_len)
    {
        if (connection == nullptr || connection->switcher == nullptr || connection->media_pool == nullptr || connection->stills == nullptr)
        {
            SetError(error_buffer, error_buffer_len, "invalid switcher connection");
            return kInternalError;
        }

        return kSuccess;
    }
}

int32_t atem_connect(
    const char* device_address,
    atem_connection** out_connection,
    int32_t* out_fail_reason,
    char* error_buffer,
    int32_t error_buffer_len)
{
    if (out_connection == nullptr)
    {
        SetError(error_buffer, error_buffer_len, "out_connection must not be null");
        return kInternalError;
    }

    *out_connection = nullptr;
    if (out_fail_reason != nullptr)
    {
        *out_fail_reason = 0;
    }

    IBMDSwitcherDiscovery* discovery = CreateDiscovery();
    if (discovery == nullptr)
    {
        SetError(error_buffer, error_buffer_len, "unable to load BMDSwitcherAPI bundle");
        return kInternalError;
    }

    CFStringRef address_cf = Utf8ToCFString(device_address);
    if (address_cf == nullptr)
    {
        discovery->Release();
        SetError(error_buffer, error_buffer_len, "failed to create address string");
        return kInternalError;
    }

    IBMDSwitcher* switcher = nullptr;
    BMDSwitcherConnectToFailure fail_reason = static_cast<BMDSwitcherConnectToFailure>(0);
    HRESULT hr = discovery->ConnectTo(address_cf, &switcher, &fail_reason);

    CFRelease(address_cf);
    discovery->Release();

    if (FAILED(hr) || switcher == nullptr)
    {
        if (out_fail_reason != nullptr)
        {
            *out_fail_reason = static_cast<int32_t>(fail_reason);
        }
        SetErrorFromHResult(error_buffer, error_buffer_len, "ConnectTo", hr);
        return static_cast<int32_t>(hr);
    }

    IBMDSwitcherMediaPool* media_pool = nullptr;
    hr = switcher->QueryInterface(IID_IBMDSwitcherMediaPool, reinterpret_cast<void**>(&media_pool));
    if (FAILED(hr) || media_pool == nullptr)
    {
        switcher->Release();
        SetErrorFromHResult(error_buffer, error_buffer_len, "QueryInterface(IBMDSwitcherMediaPool)", hr);
        return static_cast<int32_t>(hr);
    }

    IBMDSwitcherStills* stills = nullptr;
    hr = media_pool->GetStills(&stills);
    if (FAILED(hr) || stills == nullptr)
    {
        media_pool->Release();
        switcher->Release();
        SetErrorFromHResult(error_buffer, error_buffer_len, "GetStills", hr);
        return static_cast<int32_t>(hr);
    }

    atem_connection* connection = new atem_connection();
    connection->switcher = switcher;
    connection->media_pool = media_pool;
    connection->stills = stills;

    *out_connection = connection;
    return kSuccess;
}

void atem_disconnect(atem_connection* connection)
{
    if (connection == nullptr)
    {
        return;
    }

    if (connection->stills != nullptr)
    {
        connection->stills->Release();
        connection->stills = nullptr;
    }

    if (connection->media_pool != nullptr)
    {
        connection->media_pool->Release();
        connection->media_pool = nullptr;
    }

    if (connection->switcher != nullptr)
    {
        connection->switcher->Release();
        connection->switcher = nullptr;
    }

    delete connection;
}

int32_t atem_get_product_name(
    atem_connection* connection,
    char* out_name,
    int32_t out_name_len,
    char* error_buffer,
    int32_t error_buffer_len)
{
    if (out_name == nullptr || out_name_len < 1)
    {
        SetError(error_buffer, error_buffer_len, "out_name buffer is invalid");
        return kInternalError;
    }

    int32_t status = EnsureConnection(connection, error_buffer, error_buffer_len);
    if (status != kSuccess)
    {
        return status;
    }

    CFStringRef product_name = nullptr;
    HRESULT hr = connection->switcher->GetProductName(&product_name);
    if (FAILED(hr) || product_name == nullptr)
    {
        SetErrorFromHResult(error_buffer, error_buffer_len, "GetProductName", hr);
        return static_cast<int32_t>(hr);
    }

    std::string utf8 = CFStringToUtf8(product_name);
    CFRelease(product_name);

    std::snprintf(out_name, static_cast<size_t>(out_name_len), "%s", utf8.c_str());
    return kSuccess;
}

int32_t atem_get_video_mode(
    atem_connection* connection,
    int32_t* out_video_mode,
    char* error_buffer,
    int32_t error_buffer_len)
{
    if (out_video_mode == nullptr)
    {
        SetError(error_buffer, error_buffer_len, "out_video_mode must not be null");
        return kInternalError;
    }

    int32_t status = EnsureConnection(connection, error_buffer, error_buffer_len);
    if (status != kSuccess)
    {
        return status;
    }

    BMDSwitcherVideoMode mode = static_cast<BMDSwitcherVideoMode>(0);
    HRESULT hr = connection->switcher->GetVideoMode(&mode);
    if (FAILED(hr))
    {
        SetErrorFromHResult(error_buffer, error_buffer_len, "GetVideoMode", hr);
        return static_cast<int32_t>(hr);
    }

    *out_video_mode = static_cast<int32_t>(mode);
    return kSuccess;
}

int32_t atem_get_video_dimensions(
    atem_connection* connection,
    int32_t* out_width,
    int32_t* out_height,
    char* error_buffer,
    int32_t error_buffer_len)
{
    if (out_width == nullptr || out_height == nullptr)
    {
        SetError(error_buffer, error_buffer_len, "out_width/out_height must not be null");
        return kInternalError;
    }

    int32_t mode_value = 0;
    int32_t result = atem_get_video_mode(connection, &mode_value, error_buffer, error_buffer_len);
    if (result != kSuccess)
    {
        return result;
    }
    BMDSwitcherVideoMode mode = static_cast<BMDSwitcherVideoMode>(mode_value);

    switch (mode)
    {
        case bmdSwitcherVideoMode4KHDp2398:
        case bmdSwitcherVideoMode4KHDp24:
        case bmdSwitcherVideoMode4KHDp25:
        case bmdSwitcherVideoMode4KHDp2997:
        case bmdSwitcherVideoMode4KHDp30:
        case bmdSwitcherVideoMode4KHDp5994:
            *out_width = 3840;
            *out_height = 2160;
            break;
        case bmdSwitcherVideoMode720p50:
        case bmdSwitcherVideoMode720p5994:
        case bmdSwitcherVideoMode720p60:
            *out_width = 1280;
            *out_height = 720;
            break;
        case bmdSwitcherVideoMode525i5994NTSC:
            *out_width = 720;
            *out_height = 480;
            break;
        default:
            *out_width = 1920;
            *out_height = 1080;
            break;
    }

    return kSuccess;
}

int32_t atem_get_stills(
    atem_connection* connection,
    atem_still_info* out_items,
    int32_t out_items_max,
    int32_t* out_count,
    char* error_buffer,
    int32_t error_buffer_len)
{
    if (out_count == nullptr)
    {
        SetError(error_buffer, error_buffer_len, "out_count must not be null");
        return kInternalError;
    }

    int32_t status = EnsureConnection(connection, error_buffer, error_buffer_len);
    if (status != kSuccess)
    {
        return status;
    }

    uint32_t count = 0;
    HRESULT hr = connection->stills->GetCount(&count);
    if (FAILED(hr))
    {
        SetErrorFromHResult(error_buffer, error_buffer_len, "GetCount", hr);
        return static_cast<int32_t>(hr);
    }

    *out_count = static_cast<int32_t>(count);
    if (out_items == nullptr || out_items_max <= 0)
    {
        return kSuccess;
    }

    int32_t write_count = std::min(static_cast<int32_t>(count), out_items_max);

    for (int32_t i = 0; i < write_count; ++i)
    {
        out_items[i].slot = i + 1;
        out_items[i].media_player = 0;
        out_items[i].name[0] = '\0';
        out_items[i].hash[0] = '\0';

        CFStringRef name = nullptr;
        BMDSwitcherHash hash{};

        hr = connection->stills->GetName(static_cast<uint32_t>(i), &name);
        if (SUCCEEDED(hr) && name != nullptr)
        {
            std::string utf8_name = CFStringToUtf8(name);
            std::snprintf(out_items[i].name, sizeof(out_items[i].name), "%s", utf8_name.c_str());
            CFRelease(name);
        }

        hr = connection->stills->GetHash(static_cast<uint32_t>(i), &hash);
        if (SUCCEEDED(hr))
        {
            for (int j = 0; j < 16; ++j)
            {
                std::snprintf(out_items[i].hash + (j * 2), 3, "%02X", hash.data[j]);
            }
            out_items[i].hash[32] = '\0';
        }
    }

    IBMDSwitcherMediaPlayerIterator* media_iterator = nullptr;
    hr = connection->switcher->CreateIterator(IID_IBMDSwitcherMediaPlayerIterator, reinterpret_cast<void**>(&media_iterator));
    if (SUCCEEDED(hr) && media_iterator != nullptr)
    {
        IBMDSwitcherMediaPlayer* media_player = nullptr;
        int32_t media_player_index = 1;

        while (media_iterator->Next(&media_player) == S_OK && media_player != nullptr)
        {
            BMDSwitcherMediaPlayerSourceType source_type = static_cast<BMDSwitcherMediaPlayerSourceType>(0);
            uint32_t source_index = 0;
            if (SUCCEEDED(media_player->GetSource(&source_type, &source_index)) && source_type == bmdSwitcherMediaPlayerSourceTypeStill)
            {
                int32_t slot = static_cast<int32_t>(source_index) + 1;
                for (int32_t i = 0; i < write_count; ++i)
                {
                    if (out_items[i].slot == slot)
                    {
                        out_items[i].media_player = media_player_index;
                        break;
                    }
                }
            }

            media_player->Release();
            media_player = nullptr;
            media_player_index++;
        }

        media_iterator->Release();
    }

    return kSuccess;
}

int32_t atem_upload_still_bgra(
    atem_connection* connection,
    int32_t slot_zero_based,
    const char* name,
    const uint8_t* bgra_pixels,
    int32_t pixel_count,
    int32_t width,
    int32_t height,
    char* error_buffer,
    int32_t error_buffer_len)
{
    if (bgra_pixels == nullptr || pixel_count <= 0 || width <= 0 || height <= 0)
    {
        SetError(error_buffer, error_buffer_len, "invalid pixel buffer");
        return kInternalError;
    }

    int32_t status = EnsureConnection(connection, error_buffer, error_buffer_len);
    if (status != kSuccess)
    {
        return status;
    }

    IBMDSwitcherFrame* frame = nullptr;
    HRESULT hr = connection->media_pool->CreateFrame(
        bmdSwitcherPixelFormat8BitARGB,
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height),
        &frame);

    if (FAILED(hr) || frame == nullptr)
    {
        SetErrorFromHResult(error_buffer, error_buffer_len, "CreateFrame", hr);
        return static_cast<int32_t>(hr);
    }

    void* destination = nullptr;
    hr = frame->GetBytes(&destination);
    if (FAILED(hr) || destination == nullptr)
    {
        frame->Release();
        SetErrorFromHResult(error_buffer, error_buffer_len, "GetBytes", hr);
        return static_cast<int32_t>(hr);
    }

    std::memcpy(destination, bgra_pixels, static_cast<size_t>(pixel_count));

    auto* lock_callback = new UploadLockCallback();
    auto* stills_callback = new UploadStillsCallback();

    hr = connection->stills->AddCallback(stills_callback);
    if (FAILED(hr))
    {
        frame->Release();
        stills_callback->Release();
        lock_callback->Release();
        SetErrorFromHResult(error_buffer, error_buffer_len, "AddCallback", hr);
        return static_cast<int32_t>(hr);
    }

    hr = connection->stills->Lock(lock_callback);
    if (FAILED(hr))
    {
        connection->stills->RemoveCallback(stills_callback);
        frame->Release();
        stills_callback->Release();
        lock_callback->Release();
        SetErrorFromHResult(error_buffer, error_buffer_len, "Lock", hr);
        return static_cast<int32_t>(hr);
    }

    if (!lock_callback->WaitForObtained(std::chrono::seconds(5)))
    {
        connection->stills->RemoveCallback(stills_callback);
        connection->stills->Unlock(lock_callback);
        frame->Release();
        stills_callback->Release();
        lock_callback->Release();
        SetError(error_buffer, error_buffer_len, "timed out waiting for media pool lock");
        return kTimeoutError;
    }

    CFStringRef name_cf = Utf8ToCFString(name != nullptr ? name : "upload");
    hr = connection->stills->Upload(static_cast<uint32_t>(slot_zero_based), name_cf, frame);
    if (name_cf != nullptr)
    {
        CFRelease(name_cf);
    }

    if (FAILED(hr))
    {
        connection->stills->RemoveCallback(stills_callback);
        connection->stills->Unlock(lock_callback);
        frame->Release();
        stills_callback->Release();
        lock_callback->Release();
        SetErrorFromHResult(error_buffer, error_buffer_len, "Upload", hr);
        return static_cast<int32_t>(hr);
    }

    if (!stills_callback->WaitForCompleted(std::chrono::seconds(60)))
    {
        connection->stills->CancelTransfer();
        connection->stills->RemoveCallback(stills_callback);
        connection->stills->Unlock(lock_callback);
        frame->Release();
        stills_callback->Release();
        lock_callback->Release();
        SetError(error_buffer, error_buffer_len, "timed out waiting for upload completion");
        return kTimeoutError;
    }

    connection->stills->RemoveCallback(stills_callback);
    connection->stills->Unlock(lock_callback);

    frame->Release();
    stills_callback->Release();
    lock_callback->Release();

    return kSuccess;
}
