using System;
using System.Reflection;

namespace SwitcherLib
{
    public class ConsoleUtils
    {
        public static void Version()
        {
            Assembly entryAssembly = Assembly.GetEntryAssembly() ?? Assembly.GetExecutingAssembly();
            Version version = entryAssembly.GetName().Version ?? new Version(1, 0, 0, 0);
            AssemblyTitleAttribute title = entryAssembly.GetCustomAttribute<AssemblyTitleAttribute>() ?? new AssemblyTitleAttribute(entryAssembly.GetName().Name ?? "atemlib");
            Console.Out.WriteLine(String.Format("{0} {1}.{2}.{3}", title.Title, version.Major.ToString(), version.Minor.ToString(), version.Revision.ToString()));
            Console.Out.WriteLine("Jessica Smith <jess@mintopia.net>");
            Console.Out.WriteLine("This software is released under the MIT License");
        }
    }
}
