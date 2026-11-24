// RUN: %dxreflector %s | FileCheck %s

void test(uint b) {

	if(uint d0 = b ^ 0x13) {
		float c0;
	}

	else {
		float c1;
	}

	if(uint d1 = b ^ 0x13) {
		float c2;
	}

	else if(uint d2 = b ^ 0x12) {
		float c3;
		uint e;
		int f;
	}

	else {
		float c4;
	}

	if(false) {
		float c20;
	}

	else if(false) {
		float c21;
	}

	if(true) {
		float c25;
		if(true) {
			float c24;
		}
		else {
			if(true) {
				float c22;
			}
			float c23;
		}
	}

	while(true) {
		float c5;
		break;
	}

	while(uint d3 = b ^ 0x13) {
		float c6;
		break;
	}

	do {
		float c7;
		break;
	} while(b ^ 0x13);

	{
		float c8;
	}

	for(uint i = 0; i < 5; ++i) {
		float c9;
	}

	[unroll]
	for(uint i0 = 0; i0 < 2; ++i0) {
		float c10;
	}

	for(uint i1 = 0, j = 5; bool k = i1 < 2; ++i1, ++j) {
		float c11;
	}

	if(b == 0) {
		float c12;
	}
	else if(b == 1) {
		float c13;
	}
	else if(b == 2) {
		float c14;
	}
	else if(b == 3) {
		float c15;
	}
	else {
		float c16;
		float c17;
	}

	switch(uint d4 = b ^ 0x13) {
		case 0:
			float c17;
			break;
	}

	switch(b) {
		case 0:
			break;
		default:
			break;
	}

	switch(b) {
		default:
			break;
	}

	switch(b) {
		case 0: {
			float c18;
			break;
		}
		case 1: {
			float c19;
			break;
		}
		case 2: {
			float c20;
			break;
		}
		case 3: {
			float c21;
			break;
		}
		default: {
			float c22;
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
// CHECK:                                         "NodeType": "IfRoot",
// CHECK:                                         "Children": [
// CHECK:                                                 {
// CHECK:                                                         "NodeType": "IfFirst",
// CHECK:                                                         "Branch": {
// CHECK:                                                                 "Condition": {
// CHECK:                                                                         "Name": "d0",
// CHECK:                                                                         "NodeType": "Variable",
// CHECK:                                                                         "Type": {
// CHECK:                                                                                 "Name": "uint"
// CHECK:                                                                         }
// CHECK:                                                                 }
// CHECK:                                                         },
// CHECK:                                                         "Children": [
// CHECK:                                                                 {
// CHECK:                                                                         "Name": "c0",
// CHECK:                                                                         "NodeType": "Variable",
// CHECK:                                                                         "Type": {
// CHECK:                                                                                 "Name": "float"
// CHECK:                                                                         }
// CHECK:                                                                 }
// CHECK:                                                         ]
// CHECK:                                                 },
// CHECK:                                                 {
// CHECK:                                                         "NodeType": "Else",
// CHECK:                                                         "Children": [
// CHECK:                                                                 {
// CHECK:                                                                         "Name": "c1",
// CHECK:                                                                         "NodeType": "Variable",
// CHECK:                                                                         "Type": {
// CHECK:                                                                                 "Name": "float"
// CHECK:                                                                         }
// CHECK:                                                                 }
// CHECK:                                                         ]
// CHECK:                                                 }
// CHECK:                                         ]
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "NodeType": "IfRoot",
// CHECK:                                         "Children": [
// CHECK:                                                 {
// CHECK:                                                         "NodeType": "IfFirst",
// CHECK:                                                         "Branch": {
// CHECK:                                                                 "Condition": {
// CHECK:                                                                         "Name": "d1",
// CHECK:                                                                         "NodeType": "Variable",
// CHECK:                                                                         "Type": {
// CHECK:                                                                                 "Name": "uint"
// CHECK:                                                                         }
// CHECK:                                                                 }
// CHECK:                                                         },
// CHECK:                                                         "Children": [
// CHECK:                                                                 {
// CHECK:                                                                         "Name": "c2",
// CHECK:                                                                         "NodeType": "Variable",
// CHECK:                                                                         "Type": {
// CHECK:                                                                                 "Name": "float"
// CHECK:                                                                         }
// CHECK:                                                                 }
// CHECK:                                                         ]
// CHECK:                                                 },
// CHECK:                                                 {
// CHECK:                                                         "NodeType": "ElseIf",
// CHECK:                                                         "Branch": {
// CHECK:                                                                 "Condition": {
// CHECK:                                                                         "Name": "d2",
// CHECK:                                                                         "NodeType": "Variable",
// CHECK:                                                                         "Type": {
// CHECK:                                                                                 "Name": "uint"
// CHECK:                                                                         }
// CHECK:                                                                 }
// CHECK:                                                         },
// CHECK:                                                         "Children": [
// CHECK:                                                                 {
// CHECK:                                                                         "Name": "c3",
// CHECK:                                                                         "NodeType": "Variable",
// CHECK:                                                                         "Type": {
// CHECK:                                                                                 "Name": "float"
// CHECK:                                                                         }
// CHECK:                                                                 },
// CHECK:                                                                 {
// CHECK:                                                                         "Name": "e",
// CHECK:                                                                         "NodeType": "Variable",
// CHECK:                                                                         "Type": {
// CHECK:                                                                                 "Name": "uint"
// CHECK:                                                                         }
// CHECK:                                                                 },
// CHECK:                                                                 {
// CHECK:                                                                         "Name": "f",
// CHECK:                                                                         "NodeType": "Variable",
// CHECK:                                                                         "Type": {
// CHECK:                                                                                 "Name": "int"
// CHECK:                                                                         }
// CHECK:                                                                 }
// CHECK:                                                         ]
// CHECK:                                                 },
// CHECK:                                                 {
// CHECK:                                                         "NodeType": "Else",
// CHECK:                                                         "Children": [
// CHECK:                                                                 {
// CHECK:                                                                         "Name": "c4",
// CHECK:                                                                         "NodeType": "Variable",
// CHECK:                                                                         "Type": {
// CHECK:                                                                                 "Name": "float"
// CHECK:                                                                         }
// CHECK:                                                                 }
// CHECK:                                                         ]
// CHECK:                                                 }
// CHECK:                                         ]
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "NodeType": "IfRoot",
// CHECK:                                         "Children": [
// CHECK:                                                 {
// CHECK:                                                         "NodeType": "IfFirst",
// CHECK:                                                         "Children": [
// CHECK:                                                                 {
// CHECK:                                                                         "Name": "c20",
// CHECK:                                                                         "NodeType": "Variable",
// CHECK:                                                                         "Type": {
// CHECK:                                                                                 "Name": "float"
// CHECK:                                                                         }
// CHECK:                                                                 }
// CHECK:                                                         ]
// CHECK:                                                 },
// CHECK:                                                 {
// CHECK:                                                         "NodeType": "ElseIf",
// CHECK:                                                         "Children": [
// CHECK:                                                                 {
// CHECK:                                                                         "Name": "c21",
// CHECK:                                                                         "NodeType": "Variable",
// CHECK:                                                                         "Type": {
// CHECK:                                                                                 "Name": "float"
// CHECK:                                                                         }
// CHECK:                                                                 }
// CHECK:                                                         ]
// CHECK:                                                 }
// CHECK:                                         ]
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "NodeType": "IfRoot",
// CHECK:                                         "Children": [
// CHECK:                                                 {
// CHECK:                                                         "NodeType": "IfFirst",
// CHECK:                                                         "Children": [
// CHECK:                                                                 {
// CHECK:                                                                         "Name": "c25",
// CHECK:                                                                         "NodeType": "Variable",
// CHECK:                                                                         "Type": {
// CHECK:                                                                                 "Name": "float"
// CHECK:                                                                         }
// CHECK:                                                                 },
// CHECK:                                                                 {
// CHECK:                                                                         "NodeType": "IfRoot",
// CHECK:                                                                         "Children": [
// CHECK:                                                                                 {
// CHECK:                                                                                         "NodeType": "IfFirst",
// CHECK:                                                                                         "Children": [
// CHECK:                                                                                                 {
// CHECK:                                                                                                         "Name": "c24",
// CHECK:                                                                                                         "NodeType": "Variable",
// CHECK:                                                                                                         "Type": {
// CHECK:                                                                                                                 "Name": "float"
// CHECK:                                                                                                         }
// CHECK:                                                                                                 }
// CHECK:                                                                                         ]
// CHECK:                                                                                 },
// CHECK:                                                                                 {
// CHECK:                                                                                         "NodeType": "Else",
// CHECK:                                                                                         "Children": [
// CHECK:                                                                                                 {
// CHECK:                                                                                                         "NodeType": "IfRoot",
// CHECK:                                                                                                         "Children": [
// CHECK:                                                                                                                 {
// CHECK:                                                                                                                         "NodeType": "IfFirst",
// CHECK:                                                                                                                         "Children": [
// CHECK:                                                                                                                                 {
// CHECK:                                                                                                                                         "Name": "c22",
// CHECK:                                                                                                                                         "NodeType": "Variable",
// CHECK:                                                                                                                                         "Type": {
// CHECK:                                                                                                                                                 "Name": "float"
// CHECK:                                                                                                                                         }
// CHECK:                                                                                                                                 }
// CHECK:                                                                                                                         ]
// CHECK:                                                                                                                 }
// CHECK:                                                                                                         ]
// CHECK:                                                                                                 },
// CHECK:                                                                                                 {
// CHECK:                                                                                                         "Name": "c23",
// CHECK:                                                                                                         "NodeType": "Variable",
// CHECK:                                                                                                         "Type": {
// CHECK:                                                                                                                 "Name": "float"
// CHECK:                                                                                                         }
// CHECK:                                                                                                 }
// CHECK:                                                                                         ]
// CHECK:                                                                                 }
// CHECK:                                                                         ]
// CHECK:                                                                 }
// CHECK:                                                         ]
// CHECK:                                                 }
// CHECK:                                         ]
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "NodeType": "While",
// CHECK:                                         "While": {
// CHECK:                                                 "Body": [
// CHECK:                                                         {
// CHECK:                                                                 "Name": "c5",
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
// CHECK:                                                         "Name": "d3",
// CHECK:                                                         "NodeType": "Variable",
// CHECK:                                                         "Type": {
// CHECK:                                                                 "Name": "uint"
// CHECK:                                                         }
// CHECK:                                                 },
// CHECK:                                                 "Body": [
// CHECK:                                                         {
// CHECK:                                                                 "Name": "c6",
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
// CHECK:                                                         "Name": "c7",
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
// CHECK:                                                         "Name": "c8",
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
// CHECK:                                                                 "Name": "c9",
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
// CHECK:                                                                 "Name": "i0",
// CHECK:                                                                 "NodeType": "Variable",
// CHECK:                                                                 "Type": {
// CHECK:                                                                         "Name": "uint"
// CHECK:                                                                 }
// CHECK:                                                         }
// CHECK:                                                 ],
// CHECK:                                                 "Body": [
// CHECK:                                                         {
// CHECK:                                                                 "Name": "c10",
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
// CHECK:                                                                 "Name": "i1",
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
// CHECK:                                                                 "Name": "c11",
// CHECK:                                                                 "NodeType": "Variable",
// CHECK:                                                                 "Type": {
// CHECK:                                                                         "Name": "float"
// CHECK:                                                                 }
// CHECK:                                                         }
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "NodeType": "IfRoot",
// CHECK:                                         "Children": [
// CHECK:                                                 {
// CHECK:                                                         "NodeType": "IfFirst",
// CHECK:                                                         "Children": [
// CHECK:                                                                 {
// CHECK:                                                                         "Name": "c12",
// CHECK:                                                                         "NodeType": "Variable",
// CHECK:                                                                         "Type": {
// CHECK:                                                                                 "Name": "float"
// CHECK:                                                                         }
// CHECK:                                                                 }
// CHECK:                                                         ]
// CHECK:                                                 },
// CHECK:                                                 {
// CHECK:                                                         "NodeType": "ElseIf",
// CHECK:                                                         "Children": [
// CHECK:                                                                 {
// CHECK:                                                                         "Name": "c13",
// CHECK:                                                                         "NodeType": "Variable",
// CHECK:                                                                         "Type": {
// CHECK:                                                                                 "Name": "float"
// CHECK:                                                                         }
// CHECK:                                                                 }
// CHECK:                                                         ]
// CHECK:                                                 },
// CHECK:                                                 {
// CHECK:                                                         "NodeType": "ElseIf",
// CHECK:                                                         "Children": [
// CHECK:                                                                 {
// CHECK:                                                                         "Name": "c14",
// CHECK:                                                                         "NodeType": "Variable",
// CHECK:                                                                         "Type": {
// CHECK:                                                                                 "Name": "float"
// CHECK:                                                                         }
// CHECK:                                                                 }
// CHECK:                                                         ]
// CHECK:                                                 },
// CHECK:                                                 {
// CHECK:                                                         "NodeType": "ElseIf",
// CHECK:                                                         "Children": [
// CHECK:                                                                 {
// CHECK:                                                                         "Name": "c15",
// CHECK:                                                                         "NodeType": "Variable",
// CHECK:                                                                         "Type": {
// CHECK:                                                                                 "Name": "float"
// CHECK:                                                                         }
// CHECK:                                                                 }
// CHECK:                                                         ]
// CHECK:                                                 },
// CHECK:                                                 {
// CHECK:                                                         "NodeType": "Else",
// CHECK:                                                         "Children": [
// CHECK:                                                                 {
// CHECK:                                                                         "Name": "c16",
// CHECK:                                                                         "NodeType": "Variable",
// CHECK:                                                                         "Type": {
// CHECK:                                                                                 "Name": "float"
// CHECK:                                                                         }
// CHECK:                                                                 },
// CHECK:                                                                 {
// CHECK:                                                                         "Name": "c17",
// CHECK:                                                                         "NodeType": "Variable",
// CHECK:                                                                         "Type": {
// CHECK:                                                                                 "Name": "float"
// CHECK:                                                                         }
// CHECK:                                                                 }
// CHECK:                                                         ]
// CHECK:                                                 }
// CHECK:                                         ]
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "NodeType": "Switch",
// CHECK:                                         "Switch": {
// CHECK:                                                 "Condition": {
// CHECK:                                                         "Name": "d4",
// CHECK:                                                         "NodeType": "Variable",
// CHECK:                                                         "Type": {
// CHECK:                                                                 "Name": "uint"
// CHECK:                                                         }
// CHECK:                                                 }
// CHECK:                                         },
// CHECK:                                         "Children": [
// CHECK:                                                 {
// CHECK:                                                         "NodeType": "Case",
// CHECK:                                                         "Case": {
// CHECK:                                                                 "Type": "uint",
// CHECK:                                                                 "Value": 0
// CHECK:                                                         }
// CHECK:                                                 }
// CHECK:                                         ]
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "NodeType": "Switch",
// CHECK:                                         "Children": [
// CHECK:                                                 {
// CHECK:                                                         "NodeType": "Case",
// CHECK:                                                         "Case": {
// CHECK:                                                                 "Type": "uint",
// CHECK:                                                                 "Value": 0
// CHECK:                                                         }
// CHECK:                                                 },
// CHECK:                                                 {
// CHECK:                                                         "NodeType": "Default"
// CHECK:                                                 }
// CHECK:                                         ]
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "NodeType": "Switch",
// CHECK:                                         "Children": [
// CHECK:                                                 {
// CHECK:                                                         "NodeType": "Default"
// CHECK:                                                 }
// CHECK:                                         ]
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "NodeType": "Switch",
// CHECK:                                         "Children": [
// CHECK:                                                 {
// CHECK:                                                         "NodeType": "Case",
// CHECK:                                                         "Case": {
// CHECK:                                                                 "Type": "uint",
// CHECK:                                                                 "Value": 0
// CHECK:                                                         },
// CHECK:                                                         "Children": [
// CHECK:                                                                 {
// CHECK:                                                                         "Name": "c18",
// CHECK:                                                                         "NodeType": "Variable",
// CHECK:                                                                         "Type": {
// CHECK:                                                                                 "Name": "float"
// CHECK:                                                                         }
// CHECK:                                                                 }
// CHECK:                                                         ]
// CHECK:                                                 },
// CHECK:                                                 {
// CHECK:                                                         "NodeType": "Case",
// CHECK:                                                         "Case": {
// CHECK:                                                                 "Type": "uint",
// CHECK:                                                                 "Value": 1
// CHECK:                                                         },
// CHECK:                                                         "Children": [
// CHECK:                                                                 {
// CHECK:                                                                         "Name": "c19",
// CHECK:                                                                         "NodeType": "Variable",
// CHECK:                                                                         "Type": {
// CHECK:                                                                                 "Name": "float"
// CHECK:                                                                         }
// CHECK:                                                                 }
// CHECK:                                                         ]
// CHECK:                                                 },
// CHECK:                                                 {
// CHECK:                                                         "NodeType": "Case",
// CHECK:                                                         "Case": {
// CHECK:                                                                 "Type": "uint",
// CHECK:                                                                 "Value": 2
// CHECK:                                                         },
// CHECK:                                                         "Children": [
// CHECK:                                                                 {
// CHECK:                                                                         "Name": "c20",
// CHECK:                                                                         "NodeType": "Variable",
// CHECK:                                                                         "Type": {
// CHECK:                                                                                 "Name": "float"
// CHECK:                                                                         }
// CHECK:                                                                 }
// CHECK:                                                         ]
// CHECK:                                                 },
// CHECK:                                                 {
// CHECK:                                                         "NodeType": "Case",
// CHECK:                                                         "Case": {
// CHECK:                                                                 "Type": "uint",
// CHECK:                                                                 "Value": 3
// CHECK:                                                         },
// CHECK:                                                         "Children": [
// CHECK:                                                                 {
// CHECK:                                                                         "Name": "c21",
// CHECK:                                                                         "NodeType": "Variable",
// CHECK:                                                                         "Type": {
// CHECK:                                                                                 "Name": "float"
// CHECK:                                                                         }
// CHECK:                                                                 }
// CHECK:                                                         ]
// CHECK:                                                 },
// CHECK:                                                 {
// CHECK:                                                         "NodeType": "Default",
// CHECK:                                                         "Children": [
// CHECK:                                                                 {
// CHECK:                                                                         "Name": "c22",
// CHECK:                                                                         "NodeType": "Variable",
// CHECK:                                                                         "Type": {
// CHECK:                                                                                 "Name": "float"
// CHECK:                                                                         }
// CHECK:                                                                 }
// CHECK:                                                         ]
// CHECK:                                                 }
// CHECK:                                         ]
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 }
// CHECK:         ]
// CHECK: }
