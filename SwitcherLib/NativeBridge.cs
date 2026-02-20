using System;
using System.Runtime.InteropServices;

namespace SwitcherLib
{
    internal static class NativeBridge
    {
        private const string LibraryName = "atem_bridge";

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
        internal struct NativeStillInfo
        {
            public int Slot;
            public int MediaPlayer;

            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)]
            public string Name;

            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 33)]
            public string Hash;
        }

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        internal static extern int atem_connect(
            string deviceAddress,
            out IntPtr connection,
            out int failReason,
            IntPtr errorBuffer,
            int errorBufferLength);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void atem_disconnect(IntPtr connection);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int atem_get_product_name(
            IntPtr connection,
            IntPtr outName,
            int outNameLength,
            IntPtr errorBuffer,
            int errorBufferLength);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int atem_get_video_dimensions(
            IntPtr connection,
            out int outWidth,
            out int outHeight,
            IntPtr errorBuffer,
            int errorBufferLength);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int atem_get_stills(
            IntPtr connection,
            [Out] NativeStillInfo[] outItems,
            int outItemsMax,
            out int outCount,
            IntPtr errorBuffer,
            int errorBufferLength);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        internal static extern int atem_upload_still_bgra(
            IntPtr connection,
            int slotZeroBased,
            string name,
            byte[] bgraPixels,
            int pixelCount,
            int width,
            int height,
            IntPtr errorBuffer,
            int errorBufferLength);

        internal static string ReadAnsiBuffer(IntPtr ptr)
        {
            return Marshal.PtrToStringAnsi(ptr) ?? string.Empty;
        }
    }
}
