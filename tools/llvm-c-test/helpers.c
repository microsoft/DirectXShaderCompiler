/*===-- helpers.c - tool for testing libLLVM and llvm-c API ---------------===*\
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// helpers.c                                                                 //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// Helper functions                                                          //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm-c-test.h"
#include <stdio.h>
#include <string.h>

#define MAX_TOKENS 512
#define MAX_LINE_LEN 1024

void tokenize_stdin(void (*cb)(char **tokens, int ntokens)) {
  char line[MAX_LINE_LEN];
  char *tokbuf[MAX_TOKENS];

  while (fgets(line, sizeof(line), stdin)) {
    int c = 0;

    if (line[0] == ';' || line[0] == '\n')
      continue;

    while (c < MAX_TOKENS) {
      tokbuf[c] = strtok(c ? NULL : line, " \n");
      if (!tokbuf[c])
        break;
      c++;
    }
    if (c)
      cb(tokbuf, c);
  }
}
