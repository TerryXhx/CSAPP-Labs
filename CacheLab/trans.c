/* Reference : https://yangtau.me/computer-system/csapp-cache.html */
/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>

#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

int min(int num1, int num2);
void trans_32x32(int A[32][32], int B[32][32]);
void trans_64x64(int A[64][64], int B[64][64]);
void trans_67x61(int A[67][61], int B[61][67]);
void trans(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N]) {
    if (N == 32 && M == 32) 
        trans_32x32(A, B);
    else if (N == 64 && M == 64)
        trans_64x64(A, B);
    else if (N == 67 && M == 61)
        trans_67x61(A, B);
    else
        trans(M, N, A, B);
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N]) {
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

char trans_32x32_desc[] = "Transpose for 32 x 32 matrix";
void trans_32x32(int A[32][32], int B[32][32]) {
    /* 
    * 13 local variables
    * `BLOCKSIZE` can actually be removed, defined just for readability.
    */
    const int BLOCKSIZE = 8;
    int i, j, k, l;
    int a0, a1, a2, a3, a4, a5, a6, a7;
    
    for (i = 0; i < 32; i += BLOCKSIZE) {
        for (j = 0; j < 32; j += BLOCKSIZE) {
            /* Copy the block from A to B in row order */
            for (k = i, l = j; k < i + BLOCKSIZE && l < j + BLOCKSIZE; ++k, ++l) {
                a0 = A[k][j];
                a1 = A[k][j + 1];
                a2 = A[k][j + 2];
                a3 = A[k][j + 3];
                a4 = A[k][j + 4];
                a5 = A[k][j + 5];
                a6 = A[k][j + 6];
                a7 = A[k][j + 7];

                B[l][i] = a0;
                B[l][i + 1] = a1;
                B[l][i + 2] = a2;
                B[l][i + 3] = a3;
                B[l][i + 4] = a4;
                B[l][i + 5] = a5;
                B[l][i + 6] = a6;
                B[l][i + 7] = a7;
            }

            /* Transpose the block in matrix B */
            for (k = 0; k < BLOCKSIZE; ++k) {
                for (l = k + 1; l < BLOCKSIZE; ++l) {
                    a0 = B[j + k][i + l];
                    B[j + k][i + l] = B[j + l][i + k];
                    B[j + l][i + k] = a0;
                }
            }
        }
    }
}

char trans_64x64_desc[] = "Transpose for 64 x 64 matrix";
void trans_64x64(int A[64][64], int B[64][64]) {
    /* 
    * 13 local variables
    * `BLOCKSIZE` can actually be removed, defined just for readability.
    */
    const int BLOCKSIZE = 8;
    int i, j, k;
    int a0, a1, a2, a3, a4, a5, a6, a7, tmp;
    
    for (i = 0; i < 64; i += BLOCKSIZE) {
        for (j = 0; j < 64; j += BLOCKSIZE) {
            /* Copy the first 4 row of A to B */
            for (k = 0; k < BLOCKSIZE / 2; ++k) {
                /* A top left corner */
                a0 = A[i + k][j];
                a1 = A[i + k][j + 1];
                a2 = A[i + k][j + 2];
                a3 = A[i + k][j + 3];

                /* A top right corner */
                a4 = A[i + k][j + 4];
                a5 = A[i + k][j + 5];
                a6 = A[i + k][j + 6];
                a7 = A[i + k][j + 7];

                /* B top left corner */
                B[j][i + k]= a0;
                B[j + 1][i + k] = a1;
                B[j + 2][i + k] = a2;
                B[j + 3][i + k] = a3;

                /* B top right corner */
                B[j][i + 4 + k] = a4;
                B[j + 1][i + 4 + k] = a5;
                B[j + 2][i + 4 + k] = a6;
                B[j + 3][i + 4 + k] = a7;
            }

            /* Use buffer to place the correct elements in the top right corner and bottom rows of B */
            for (k = 0; k < BLOCKSIZE / 2; ++k) {
                /* Buffer 1 */
                a0 = A[i + 4][j + k];
                a1 = A[i + 5][j + k];
                a2 = A[i + 6][j + k];
                a3 = A[i + 7][j + k];

                /* Buffer 2 */
                a4 = A[i + 4][j + 4 + k];
                a5 = A[i + 5][j + 4 + k];
                a6 = A[i + 6][j + 4 + k];
                a7 = A[i + 7][j + 4 + k];
                
                /* Buffer 1 <--> Top right corner of B */
                tmp = B[j + k][i + 4]; B[j + k][i + 4] = a0; a0 = tmp;
                tmp = B[j + k][i + 5]; B[j + k][i + 5] = a1; a1 = tmp;
                tmp = B[j + k][i + 6]; B[j + k][i + 6] = a2; a2 = tmp;
                tmp = B[j + k][i + 7]; B[j + k][i + 7] = a3; a3 = tmp;

                /* Put elements in buffer to the bottom row of B */
                B[j + 4 + k][i] = a0;
                B[j + 4 + k][i + 1] = a1;
                B[j + 4 + k][i + 2] = a2;
                B[j + 4 + k][i + 3] = a3;
                B[j + 4 + k][i + 4] = a4;
                B[j + 4 + k][i + 5] = a5;
                B[j + 4 + k][i + 6] = a6;
                B[j + 4 + k][i + 7] = a7;
            }
        }
    } 
}

inline int min(int num1, int num2) {
    return num1 < num2 ? num1 : num2;
}

char trans_67x61_desc[] = "Transpose for 67 x 61 matrix";
void trans_67x61(int A[67][61], int B[61][67]) {
    /* 
    * 5 local variables
    * `BLOCKSIZE` can actually be removed, defined just for readability.
    */
    const int BLOCKSIZE = 16;
    int i, j, k, l;
    for (i = 0; i < 67; i += BLOCKSIZE) 
        for (j = 0; j < 61; j += BLOCKSIZE) 
            for (k = i; k < i + BLOCKSIZE && k < 67; ++k) 
                for (l = j; l < j + BLOCKSIZE && l < 61; ++l) 
                    B[l][k] = A[k][l];
}
/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N]) {
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}