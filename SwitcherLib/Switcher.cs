using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace SwitcherLib
{
    public class Switcher : IDisposable
    {
        private readonly string deviceAddress;
        private bool connected;
        private IntPtr nativeConnection;

        public Switcher(string deviceAddress)
        {
            this.deviceAddress = deviceAddress;
        }

        public void Connect()
        {
            if (this.connected)
            {
                return;
            }

            const int bufferLength = 1024;
            IntPtr errorBuffer = Marshal.AllocHGlobal(bufferLength);
            IntPtr connection = IntPtr.Zero;

            try
            {
                for (int i = 0; i < bufferLength; i++)
                {
                    Marshal.WriteByte(errorBuffer, i, 0);
                }

                int failReason;
                int result = NativeBridge.atem_connect(this.deviceAddress, out connection, out failReason, errorBuffer, bufferLength);
                if (result != 0 || connection == IntPtr.Zero)
                {
                    string message = Marshal.PtrToStringAnsi(errorBuffer) ?? "Unable to connect to switcher";
                    throw new SwitcherLibException(message);
                }

                this.nativeConnection = connection;
                this.connected = true;
            }
            finally
            {
                Marshal.FreeHGlobal(errorBuffer);
            }
        }

        public string GetProductName()
        {
            this.Connect();
            const int bufferLength = 512;
            IntPtr nameBuffer = Marshal.AllocHGlobal(bufferLength);
            IntPtr errorBuffer = Marshal.AllocHGlobal(bufferLength);

            try
            {
                for (int i = 0; i < bufferLength; i++)
                {
                    Marshal.WriteByte(nameBuffer, i, 0);
                    Marshal.WriteByte(errorBuffer, i, 0);
                }

                int result = NativeBridge.atem_get_product_name(this.nativeConnection, nameBuffer, bufferLength, errorBuffer, bufferLength);
                if (result != 0)
                {
                    string message = Marshal.PtrToStringAnsi(errorBuffer) ?? "Unable to get product name";
                    throw new SwitcherLibException(message);
                }

                return NativeBridge.ReadAnsiBuffer(nameBuffer);
            }
            finally
            {
                Marshal.FreeHGlobal(nameBuffer);
                Marshal.FreeHGlobal(errorBuffer);
            }
        }

        public int GetVideoHeight()
        {
            this.Connect();
            return this.GetVideoDimensions().height;
        }

        public int GetVideoWidth()
        {
            this.Connect();
            return this.GetVideoDimensions().width;
        }

        public IList<MediaStill> GetStills()
        {
            this.Connect();
            const int bufferLength = 1024;
            IntPtr errorBuffer = Marshal.AllocHGlobal(bufferLength);

            try
            {
                for (int i = 0; i < bufferLength; i++)
                {
                    Marshal.WriteByte(errorBuffer, i, 0);
                }

                int count;
                NativeBridge.NativeStillInfo[] probe = new NativeBridge.NativeStillInfo[1];
                int result = NativeBridge.atem_get_stills(this.nativeConnection, probe, 0, out count, errorBuffer, bufferLength);
                if (result != 0)
                {
                    throw new SwitcherLibException(Marshal.PtrToStringAnsi(errorBuffer) ?? "Unable to get still count");
                }

                NativeBridge.NativeStillInfo[] nativeItems = new NativeBridge.NativeStillInfo[count];
                result = NativeBridge.atem_get_stills(this.nativeConnection, nativeItems, nativeItems.Length, out count, errorBuffer, bufferLength);
                if (result != 0)
                {
                    throw new SwitcherLibException(Marshal.PtrToStringAnsi(errorBuffer) ?? "Unable to enumerate stills");
                }

                List<MediaStill> items = new List<MediaStill>();
                for (int index = 0; index < count; index++)
                {
                    items.Add(new MediaStill
                    {
                        Slot = nativeItems[index].Slot,
                        MediaPlayer = nativeItems[index].MediaPlayer,
                        Name = nativeItems[index].Name,
                        Hash = nativeItems[index].Hash,
                    });
                }

                return items;
            }
            finally
            {
                Marshal.FreeHGlobal(errorBuffer);
            }
        }

        internal IntPtr GetNativeConnection()
        {
            this.Connect();
            return this.nativeConnection;
        }

        public void Dispose()
        {
            if (this.nativeConnection != IntPtr.Zero)
            {
                NativeBridge.atem_disconnect(this.nativeConnection);
                this.nativeConnection = IntPtr.Zero;
                this.connected = false;
            }

            GC.SuppressFinalize(this);
        }

        ~Switcher()
        {
            this.Dispose();
        }

        private (int width, int height) GetVideoDimensions()
        {
            const int bufferLength = 512;
            IntPtr errorBuffer = Marshal.AllocHGlobal(bufferLength);

            try
            {
                for (int i = 0; i < bufferLength; i++)
                {
                    Marshal.WriteByte(errorBuffer, i, 0);
                }

                int width;
                int height;
                int result = NativeBridge.atem_get_video_dimensions(this.nativeConnection, out width, out height, errorBuffer, bufferLength);
                if (result != 0)
                {
                    throw new SwitcherLibException(Marshal.PtrToStringAnsi(errorBuffer) ?? "Unable to get video dimensions");
                }

                return (width, height);
            }
            finally
            {
                Marshal.FreeHGlobal(errorBuffer);
            }
        }
    }
}
