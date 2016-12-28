///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// hctspeak.js                                                               //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

// This script uses the Speech API to speak to the user.
//
// Useful for batch scripts or typing on the command-line to compensate for
// short attention spans (or multitasking).
//
// Usage:
//  hctspeak.js                       -- says 'Task complete'
//  hctspeak.js /say:"Hello, world!"  -- says 'Hello, world!'

eval(new ActiveXObject("Scripting.FileSystemObject").OpenTextFile(new ActiveXObject("WScript.Shell").ExpandEnvironmentStrings("%HLSL_SRC_DIR%\\utils\\hct\\hctjs.js"), 1).ReadAll());

var text = WScript.Arguments.Named.Item("say");
if (text == null) {
    if (WScript.Arguments.Unnamed.length === 1) {
        text = WScript.Arguments.Unnamed.Item(0);
    } else {
        text = "Task complete";
    }
}

Say(text);
