// Rewrite unchanged result:
void default_noPragma(int1x1 m);
void rowMajorAttribute_noPragma(row_major matrix<int, 1, 1> m);
void columnMajorAttribute_noPragma(column_major matrix<int, 1, 1> m);
void default_pragmaRowMajor(row_major matrix<int, 1, 1> m);
void rowMajorAttribute_pragmaRowMajor(row_major matrix<int, 1, 1> m);
void columnMajorAttribute_pragmaRowMajor(column_major matrix<int, 1, 1> m);
void default_pragmaColumnMajor(column_major matrix<int, 1, 1> m);
void rowMajorAttribute_pragmaColumnMajor(row_major matrix<int, 1, 1> m);
void columnMajorAttribute_pragmaColumnMajor(column_major matrix<int, 1, 1> m);
