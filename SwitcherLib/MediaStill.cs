using System;

namespace SwitcherLib
{
    public class MediaStill
    {
        public string Name;
        public string Hash;
        public int Slot;
        public int MediaPlayer;

        public string ToCSV()
        {
            return string.Join(",", this.Slot.ToString(), "\"" + this.Name + "\"", "\"" + this.Hash + "\"", this.MediaPlayer.ToString());
        }
    }
}
