///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Program.cs                                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides an entry point for a console program to exercise dxcompiler.     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

using System;
using System.Collections.Generic;
using System.Text;

using DotNetDxc;

namespace MainNs
{
    class Program
    {
        [STAThread]
        static int Main(string[] args)
        {
            string firstArg = (args.Length < 1) ? "-w" : args[0];
            if (firstArg == "-?" || firstArg == "/?" || firstArg == "-help")
            {
                Console.WriteLine("Prints out the color information for each token in a file.");
                Console.WriteLine();
                Console.WriteLine("USAGE:");
                Console.WriteLine("  dndxc.exe [-w] filename.hlsl");
                Console.WriteLine();
                Console.WriteLine("Options:");
                Console.WriteLine("  -w    window user interface mode");
                return 1;
            }

            if (firstArg == "-w")
            {
                new EditorForm().ShowDialog();
                return 0;
            }

            PrintOutTokenColors(args[0]);

            return 0;
        }

        static void PrintOutTokenColors(string path)
        {
            IDxcIntelliSense isense;
            try
            {
                isense = HlslDxcLib.CreateDxcIntelliSense();
            }
            catch (System.DllNotFoundException dllNotFound)
            {
                Console.WriteLine("Unable to create IntelliSense helper - DLL not found there.");
                Console.WriteLine(dllNotFound.ToString());
                return;
            }
            catch (Exception e)
            {
                Console.WriteLine("Unable to create IntelliSense helper.");
                Console.WriteLine(e.ToString());
                return;
            }

            IDxcIndex index = isense.CreateIndex();
            string fileName = path;
            string fileContents = System.IO.File.ReadAllText(path);
            string[] commandLineArgs = new string[] { "-ferror-limit=200" };

            IDxcUnsavedFile[] unsavedFiles = new IDxcUnsavedFile[]
            {
                new TrivialDxcUnsavedFile(fileName, fileContents)
            };

            Console.WriteLine("{0}:\n{1}", fileName, fileContents);

            IDxcTranslationUnit tu = index.ParseTranslationUnit(fileName, commandLineArgs,
                commandLineArgs.Length, unsavedFiles, (uint)unsavedFiles.Length, 0);
            if (tu == null)
            {
                Console.WriteLine("Unable to parse translation unit");
            }
            else
            {
                IDxcSourceRange range = tu.GetCursor().GetExtent();
                IDxcToken[] tokens;
                uint tokenCount;
                tu.Tokenize(range, out tokens, out tokenCount);
                Console.WriteLine("{0} tokens found.", tokenCount);
                for (UInt32 i = 0; i < tokenCount; i++)
                {
                    PrintToken(tokens[i]);
                }
            }
        }

        static string LocationToString(IDxcSourceLocation location)
        {
            IDxcFile file;
            UInt32 line, col, offset;
            location.GetSpellingLocation(out file, out line, out col, out offset);
            return String.Format("{0}:{1}", line, col);
        }

        static string RangeToString(IDxcSourceRange range)
        {
            return LocationToString(range.GetStart()) + "-" + LocationToString(range.GetEnd());
        }

        static void PrintToken(IDxcToken token)
        {
            Console.WriteLine(" '{0}' of type {1} at [{2}]",
                token.GetSpelling(), token.GetKind().ToString(), RangeToString(token.GetExtent()));
        }
    }
}
