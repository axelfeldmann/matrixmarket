#include "matrixmarket.hpp"
#include <cstdio>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <matrixmarket file>\n", argv[0]);
        return 1;
    }

    auto csr = read_csr<unsigned int,float>(argv[1]);

    for (int i = 0; i < csr.num_rows; i++) {
        for (int j = csr.row_offsets[i]; j < csr.row_offsets[i+1]; j++) {
            printf("%d %d %f\n", i, csr.col_indices[j], csr.values[j]);
        }
    }

    return 0;
}