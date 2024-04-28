#include "matrixmarket.hpp"
#include <cstdio>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <matrixmarket file>\n", argv[0]);
        return 1;
    }

    printf("CSR:\n");

    auto csr = MatrixMarket::read_csr<unsigned int,float>(argv[1]);

    printf("%u x %u, %u nnz\n", csr.num_rows, csr.num_cols, csr.num_nonzeros);


    return 0;
}
