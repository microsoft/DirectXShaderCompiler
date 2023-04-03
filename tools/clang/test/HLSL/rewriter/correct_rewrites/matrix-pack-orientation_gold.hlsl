// Rewrite unchanged result:


void default_noPragma(int1x1 m);
void rowMajorAttribute_noPragma(row_major int1x1 m);
void columnMajorAttribute_noPragma(column_major int1x1 m);

#pragma pack_matrix(row_major)
void default_pragmaRowMajor(int1x1 m);
void rowMajorAttribute_pragmaRowMajor(row_major int1x1 m);
void columnMajorAttribute_pragmaRowMajor(column_major int1x1 m);

#pragma pack_matrix(column_major)
void default_pragmaColumnMajor(int1x1 m);
void rowMajorAttribute_pragmaColumnMajor(row_major int1x1 m);
void columnMajorAttribute_pragmaColumnMajor(column_major int1x1 m);
