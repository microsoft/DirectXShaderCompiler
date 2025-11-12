// RUN: %dxreflector %s | FileCheck %s

void test(uint b) {

	if(uint d = b ^ 0x13) {
		float c;
	}

	else {
		float c;
	}

	while(true) {
		float c;
		break;
	}

	while(uint d = b ^ 0x13) {
		float c;
		break;
	}

	do {
		float c;
		break;
	} while(b ^ 0x13);

	{
		float c;
	}

	for(uint i = 0; i < 5; ++i) {
		float c;
	}

	[unroll]
	for(uint i = 0; i < 2; ++i) {
		float c;
	}

	for(uint i = 0, j = 5; bool k = i < 2; ++i, ++j) {
		float c;
	}

	//TODO: turn into a smaller chain of ifs

	if(b == 0) {
		float c;
	}
	else if(b == 1) {
		float c;
	}
	else if(b == 2) {
		float c;
	}
	else if(b == 3) {
		float c;
	}
	else {
		float c;
	}

	//TODO: Cases not working yet

	switch(uint d = b ^ 0x13) {
		case 0:
			float c;
			break;
	}

	switch(b) {
		case 0: {
			float c;
			break;
		}
		case 1: {
			float c;
			break;
		}
		case 2: {
			float c;
			break;
		}
		case 3: {
			float c;
			break;
		}
	}
}

// CHECK: {
// CHECK:         "Features": [
// CHECK:                 "Basics",
// CHECK:                 "Functions",
// CHECK:                 "Namespaces",
// CHECK:                 "UserTypes",
// CHECK:                 "Scopes",
// CHECK:                 "Symbols"
// CHECK:         ],
// CHECK:         "Children": [
// CHECK:                 {
// CHECK:                         "Name": "test",
// CHECK:                         "NodeType": "Function",
// CHECK:                         "Function": {
// CHECK:                                 "Params": {
// CHECK:                                         "b": {
// CHECK:                                                 "TypeName": "uint"
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 "ReturnType": {
// CHECK:                                         "TypeName": "void"
// CHECK:                                 }
// CHECK:                         },
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "NodeType": "If",
// CHECK:                                         "If": {
// CHECK:                                                 "Condition": {
// CHECK:                                                         "Name": "d",
// CHECK:                                                         "NodeType": "Variable",
// CHECK:                                                         "Type": {
// CHECK:                                                                 "Name": "uint"
// CHECK:                                                         }
// CHECK:                                                 },
// CHECK:                                                 "Body": [
// CHECK:                                                         {
// CHECK:                                                                 "Name": "c",
// CHECK:                                                                 "NodeType": "Variable",
// CHECK:                                                                 "Type": {
// CHECK:                                                                         "Name": "float"
// CHECK:                                                                 }
// CHECK:                                                         }
// CHECK:                                                 ],
// CHECK:                                                 "Else": [
// CHECK:                                                         {
// CHECK:                                                                 "Name": "c",
// CHECK:                                                                 "NodeType": "Variable",
// CHECK:                                                                 "Type": {
// CHECK:                                                                         "Name": "float"
// CHECK:                                                                 }
// CHECK:                                                         }
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "NodeType": "While",
// CHECK:                                         "While": {
// CHECK:                                                 "Body": [
// CHECK:                                                         {
// CHECK:                                                                 "Name": "c",
// CHECK:                                                                 "NodeType": "Variable",
// CHECK:                                                                 "Type": {
// CHECK:                                                                         "Name": "float"
// CHECK:                                                                 }
// CHECK:                                                         }
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "NodeType": "While",
// CHECK:                                         "While": {
// CHECK:                                                 "Condition": {
// CHECK:                                                         "Name": "d",
// CHECK:                                                         "NodeType": "Variable",
// CHECK:                                                         "Type": {
// CHECK:                                                                 "Name": "uint"
// CHECK:                                                         }
// CHECK:                                                 },
// CHECK:                                                 "Body": [
// CHECK:                                                         {
// CHECK:                                                                 "Name": "c",
// CHECK:                                                                 "NodeType": "Variable",
// CHECK:                                                                 "Type": {
// CHECK:                                                                         "Name": "float"
// CHECK:                                                                 }
// CHECK:                                                         }
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "NodeType": "Do",
// CHECK:                                         "Children": [
// CHECK:                                                 {
// CHECK:                                                         "Name": "c",
// CHECK:                                                         "NodeType": "Variable",
// CHECK:                                                         "Type": {
// CHECK:                                                                 "Name": "float"
// CHECK:                                                         }
// CHECK:                                                 }
// CHECK:                                         ]
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "NodeType": "Scope",
// CHECK:                                         "Children": [
// CHECK:                                                 {
// CHECK:                                                         "Name": "c",
// CHECK:                                                         "NodeType": "Variable",
// CHECK:                                                         "Type": {
// CHECK:                                                                 "Name": "float"
// CHECK:                                                         }
// CHECK:                                                 }
// CHECK:                                         ]
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "NodeType": "For",
// CHECK:                                         "For": {
// CHECK:                                                 "Init": [
// CHECK:                                                         {
// CHECK:                                                                 "Name": "i",
// CHECK:                                                                 "NodeType": "Variable",
// CHECK:                                                                 "Type": {
// CHECK:                                                                         "Name": "uint"
// CHECK:                                                                 }
// CHECK:                                                         }
// CHECK:                                                 ],
// CHECK:                                                 "Body": [
// CHECK:                                                         {
// CHECK:                                                                 "Name": "c",
// CHECK:                                                                 "NodeType": "Variable",
// CHECK:                                                                 "Type": {
// CHECK:                                                                         "Name": "float"
// CHECK:                                                                 }
// CHECK:                                                         }
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "NodeType": "For",
// CHECK:                                         "For": {
// CHECK:                                                 "Init": [
// CHECK:                                                         {
// CHECK:                                                                 "Name": "i",
// CHECK:                                                                 "NodeType": "Variable",
// CHECK:                                                                 "Type": {
// CHECK:                                                                         "Name": "uint"
// CHECK:                                                                 }
// CHECK:                                                         }
// CHECK:                                                 ],
// CHECK:                                                 "Body": [
// CHECK:                                                         {
// CHECK:                                                                 "Name": "c",
// CHECK:                                                                 "NodeType": "Variable",
// CHECK:                                                                 "Type": {
// CHECK:                                                                         "Name": "float"
// CHECK:                                                                 }
// CHECK:                                                         }
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "NodeType": "For",
// CHECK:                                         "For": {
// CHECK:                                                 "Condition": {
// CHECK:                                                         "Name": "k",
// CHECK:                                                         "NodeType": "Variable",
// CHECK:                                                         "Type": {
// CHECK:                                                                 "Name": "bool"
// CHECK:                                                         }
// CHECK:                                                 },
// CHECK:                                                 "Init": [
// CHECK:                                                         {
// CHECK:                                                                 "Name": "i",
// CHECK:                                                                 "NodeType": "Variable",
// CHECK:                                                                 "Type": {
// CHECK:                                                                         "Name": "uint"
// CHECK:                                                                 }
// CHECK:                                                         },
// CHECK:                                                         {
// CHECK:                                                                 "Name": "j",
// CHECK:                                                                 "NodeType": "Variable",
// CHECK:                                                                 "Type": {
// CHECK:                                                                         "Name": "uint"
// CHECK:                                                                 }
// CHECK:                                                         }
// CHECK:                                                 ],
// CHECK:                                                 "Body": [
// CHECK:                                                         {
// CHECK:                                                                 "Name": "c",
// CHECK:                                                                 "NodeType": "Variable",
// CHECK:                                                                 "Type": {
// CHECK:                                                                         "Name": "float"
// CHECK:                                                                 }
// CHECK:                                                         }
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "NodeType": "If",
// CHECK:                                         "If": {
// CHECK:                                                 "Body": [
// CHECK:                                                         {
// CHECK:                                                                 "Name": "c",
// CHECK:                                                                 "NodeType": "Variable",
// CHECK:                                                                 "Type": {
// CHECK:                                                                         "Name": "float"
// CHECK:                                                                 }
// CHECK:                                                         }
// CHECK:                                                 ],
// CHECK:                                                 "Else": [
// CHECK:                                                         {
// CHECK:                                                                 "NodeType": "If",
// CHECK:                                                                 "If": {
// CHECK:                                                                         "Body": [
// CHECK:                                                                                 {
// CHECK:                                                                                         "Name": "c",
// CHECK:                                                                                         "NodeType": "Variable",
// CHECK:                                                                                         "Type": {
// CHECK:                                                                                                 "Name": "float"
// CHECK:                                                                                         }
// CHECK:                                                                                 }
// CHECK:                                                                         ],
// CHECK:                                                                         "Else": [
// CHECK:                                                                                 {
// CHECK:                                                                                         "NodeType": "If",
// CHECK:                                                                                         "If": {
// CHECK:                                                                                                 "Body": [
// CHECK:                                                                                                         {
// CHECK:                                                                                                                 "Name": "c",
// CHECK:                                                                                                                 "NodeType": "Variable",
// CHECK:                                                                                                                 "Type": {
// CHECK:                                                                                                                        "Name": "float"
// CHECK:                                                                                                                 }
// CHECK:                                                                                                         }
// CHECK:                                                                                                 ],
// CHECK:                                                                                                 "Else": [
// CHECK:                                                                                                         {
// CHECK:                                                                                                                 "NodeType": "If",
// CHECK:                                                                                                                 "If": {
// CHECK:                                                                                                                        "Body": [
// CHECK:                                                                                                                        {
// CHECK:                                                                                                                        "Name": "c",
// CHECK:                                                                                                                        "NodeType": "Variable",
// CHECK:                                                                                                                        "Type": {
// CHECK:                                                                                                                        "Name": "float"
// CHECK:                                                                                                                        }
// CHECK:                                                                                                                        }
// CHECK:                                                                                                                        ],
// CHECK:                                                                                                                        "Else": [
// CHECK:                                                                                                                        {
// CHECK:                                                                                                                        "Name": "c",
// CHECK:                                                                                                                        "NodeType": "Variable",
// CHECK:                                                                                                                        "Type": {
// CHECK:                                                                                                                        "Name": "float"
// CHECK:                                                                                                                        }
// CHECK:                                                                                                                        }
// CHECK:                                                                                                                        ]
// CHECK:                                                                                                                 }
// CHECK:                                                                                                         }
// CHECK:                                                                                                 ]
// CHECK:                                                                                         }
// CHECK:                                                                                 }
// CHECK:                                                                         ]
// CHECK:                                                                 }
// CHECK:                                                         }
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "NodeType": "Switch",
// CHECK:                                         "Switch": {
// CHECK:                                                 "Condition": {
// CHECK:                                                         "Name": "d",
// CHECK:                                                         "NodeType": "Variable",
// CHECK:                                                         "Type": {
// CHECK:                                                                 "Name": "uint"
// CHECK:                                                         }
// CHECK:                                                 }
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "NodeType": "Switch",
// CHECK:                                         "Switch": {
// CHECK:                                         }
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 }
// CHECK:         ]
// CHECK: }
