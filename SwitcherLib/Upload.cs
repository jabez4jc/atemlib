using System;
using System.IO;
using System.Runtime.InteropServices;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;

namespace SwitcherLib
{
    public class Upload
    {
        private enum Status
        {
            NotStarted,
            Started,
            Completed,
        }

        private Status currentStatus;
        private readonly string filename;
        private readonly int uploadSlot;
        private string name;
        private readonly Switcher switcher;
        private int progress;

        public Upload(Switcher switcher, string filename, int uploadSlot)
        {
            this.switcher = switcher;
            this.filename = filename;
            this.uploadSlot = uploadSlot;

            if (!File.Exists(filename))
            {
                throw new SwitcherLibException(string.Format("{0} does not exist", filename));
            }

            this.switcher.Connect();
        }

        public bool InProgress()
        {
            return this.currentStatus == Status.Started;
        }

        public void SetName(string name)
        {
            this.name = name;
        }

        public int GetProgress()
        {
            return this.progress;
        }

        public void Start()
        {
            if (this.currentStatus != Status.NotStarted)
            {
                return;
            }

            this.currentStatus = Status.Started;
            this.progress = 0;
            byte[] imageData = this.ConvertImage();
            this.UploadImage(imageData);
            this.progress = 100;
            this.currentStatus = Status.Completed;
        }

        private void UploadImage(byte[] imageData)
        {
            int width = this.switcher.GetVideoWidth();
            int height = this.switcher.GetVideoHeight();

            const int bufferLength = 1024;
            IntPtr errorBuffer = Marshal.AllocHGlobal(bufferLength);

            try
            {
                for (int i = 0; i < bufferLength; i++)
                {
                    Marshal.WriteByte(errorBuffer, i, 0);
                }

                int result = NativeBridge.atem_upload_still_bgra(
                    this.switcher.GetNativeConnection(),
                    this.uploadSlot,
                    this.GetName(),
                    imageData,
                    imageData.Length,
                    width,
                    height,
                    errorBuffer,
                    bufferLength);

                if (result != 0)
                {
                    throw new SwitcherLibException(Marshal.PtrToStringAnsi(errorBuffer) ?? "Upload failed");
                }
            }
            finally
            {
                Marshal.FreeHGlobal(errorBuffer);
            }
        }

        protected byte[] ConvertImage()
        {
            try
            {
                using Image<Rgba32> image = Image.Load<Rgba32>(this.filename);

                if (image.Width != this.switcher.GetVideoWidth() || image.Height != this.switcher.GetVideoHeight())
                {
                    throw new SwitcherLibException(string.Format("Image is {0}x{1} it needs to be the same resolution as the switcher", image.Width.ToString(), image.Height.ToString()));
                }

                byte[] output = new byte[image.Width * image.Height * 4];
                for (int y = 0; y < image.Height; y++)
                {
                    for (int x = 0; x < image.Width; x++)
                    {
                        Rgba32 pixel = image[x, y];
                        int index = (y * image.Width + x) * 4;
                        output[index] = pixel.B;
                        output[index + 1] = pixel.G;
                        output[index + 2] = pixel.R;
                        output[index + 3] = pixel.A;
                    }
                }

                return output;
            }
            catch (Exception ex)
            {
                throw new SwitcherLibException(ex.Message, ex);
            }
        }

        public string GetName()
        {
            if (this.name != null)
            {
                return this.name;
            }

            return Path.GetFileNameWithoutExtension(this.filename);
        }
    }
}
