#pragma once

#include <utility>
#include <string>
#include <cstdio>
#include <cassert>
#include <vector>
#include <tuple>
#include <fstream>
#include <sstream>
#include <deque>
#include <algorithm>

namespace MatrixMarket {

///////////////////////////////////////////////////////////////////////////////
// Public API
///////////////////////////////////////////////////////////////////////////////

template<typename CoordType, typename ValueType>
struct CSRMatrix {
    CoordType num_rows;
    CoordType num_cols;
    CoordType num_nonzeros;
    std::vector<CoordType> row_offsets;
    std::vector<CoordType> col_indices;
    std::vector<ValueType> values;
};

template<typename CoordType, typename ValueType>
CSRMatrix<CoordType,ValueType> read_csr(const char* filename);

template<typename CoordType, typename ValueType>
struct CSCMatrix {
    CoordType num_rows;
    CoordType num_cols;
    CoordType num_nonzeros;
    std::vector<CoordType> col_offsets;
    std::vector<CoordType> row_indices;
    std::vector<ValueType> values;
};

template<typename CoordType, typename ValueType>
CSCMatrix<CoordType,ValueType> read_csc(const char* filename);

///////////////////////////////////////////////////////////////////////////////
// Utility struct for the header
///////////////////////////////////////////////////////////////////////////////

enum class SymmetryType { GENERAL, SYMMETRIC };
enum class ValueFormat { REAL, INTEGER, PATTERN };

template<typename CoordType>
struct Header {
    SymmetryType symmetry;
    ValueFormat value_type;
    CoordType num_rows;
    CoordType num_cols;
    CoordType num_nonzeros;
};

///////////////////////////////////////////////////////////////////////////////
// Utility Token class
///////////////////////////////////////////////////////////////////////////////

class Tokens {
public:
    Tokens(std::string& str, char sep) {
        std::istringstream iss(str);
        std::string token;
        while (std::getline(iss, token, sep)) {
            tokens.push_back(token);
        }
    }
    std::string pop() {
        assert(!tokens.empty());
        auto token = tokens.front();
        tokens.pop_front();
        return token;
    }
    std::string& peek() {
        assert(!tokens.empty());
        return tokens.front();
    }
    size_t size() { 
        return tokens.size(); 
    }
private:
    std::deque<std::string> tokens;
};

///////////////////////////////////////////////////////////////////////////////
// Utility functions
///////////////////////////////////////////////////////////////////////////////

inline ValueFormat parse_value_fmt(std::string value_fmt_string) {
    if (value_fmt_string == "real") {
        return ValueFormat::REAL;
    } else if (value_fmt_string == "integer") {
        return ValueFormat::INTEGER;
    } else if (value_fmt_string == "pattern") {
        return ValueFormat::PATTERN;
    } else {
        throw std::invalid_argument("Bad Header: unknown value format");
    }
}

inline SymmetryType parse_symmetry(std::string symmetry_string) {
    if (symmetry_string == "general") {
        return SymmetryType::GENERAL;
    } else if (symmetry_string == "symmetric") {
        return SymmetryType::SYMMETRIC;
    } else {
        throw std::invalid_argument("Bad Header: unknown symmetry");
    }
}

template<typename IntType>
IntType parse_int(std::string int_string) {
    std::istringstream iss(int_string);
    IntType int_val;
    iss >> int_val;
    return int_val;
}

template<typename NumType>
NumType parse_num(std::string num_string) {
    std::istringstream iss(num_string);
    NumType num_val;
    iss >> num_val;
    return num_val;
}

template<typename CoordType>
Header<CoordType> read_header(std::ifstream& f) {
    assert(f.is_open());

    std::string line;
    std::getline(f, line);

    auto format_tokens = Tokens(line, ' ');
    if (format_tokens.size() != 5) {
        throw std::invalid_argument("Bad Header: ill-shaped format line");
    }

    if (format_tokens.pop() != "%%MatrixMarket") {
        throw std::invalid_argument("Bad Header: missing %%%%MatrixMarket");
    }

    if (format_tokens.pop() != "matrix") {
        throw std::invalid_argument("Bad Header: only matrix supported");
    }

    if (format_tokens.pop() != "coordinate") {
        throw std::invalid_argument("Bad Header: only coordinate supported");
    }

    auto value_fmt = parse_value_fmt(format_tokens.pop());
    auto symmetry = parse_symmetry(format_tokens.pop());

    do {
        std::getline(f, line);
    } while (line[0] == '%');

    auto mtx_size_tokens = Tokens(line, ' ');
    if (mtx_size_tokens.size() != 3) {
        throw std::invalid_argument("Bad Header: missing matrix size");
    }

    auto num_rows = parse_int<CoordType>(mtx_size_tokens.pop());
    auto num_cols = parse_int<CoordType>(mtx_size_tokens.pop());
    auto num_nonzeros = parse_int<CoordType>(mtx_size_tokens.pop());

    return Header<CoordType>{
        symmetry,
        value_fmt,
        num_rows,
        num_cols,
        num_nonzeros
    };
}

///////////////////////////////////////////////////////////////////////////////
// Read CSR
///////////////////////////////////////////////////////////////////////////////

template <typename CoordType, typename ValueType>
struct Nonzero {
    CoordType row;
    CoordType col;
    ValueType value;
};

template<typename CoordType, typename ValueType>
CSRMatrix<CoordType,ValueType> read_csr(const char* filename) {

    static_assert(std::is_integral<CoordType>::value, "CoordType must be integral");
    static_assert(std::is_arithmetic<ValueType>::value, "ValueType must be arithmetic");

    std::ifstream infile(filename);
    if (!infile.is_open()) {
        throw new std::invalid_argument("Could not open file for reading");
    }
    
    auto header = read_header<CoordType>(infile);

    // Read in and populate the non-zeros in "COO format"

    using NonzeroType = Nonzero<CoordType,ValueType>;

    std::vector<NonzeroType> nonzeros;
    for (CoordType i = 0; i < header.num_nonzeros; i++) {
        std::string line;
        std::getline(infile, line);
        auto tokens = Tokens(line, ' ');

        if (header.value_type == ValueFormat::PATTERN && tokens.size() != 2) {
            throw std::invalid_argument("Bad Matrix: ill-shaped pattern line");
        } else if (header.value_type != ValueFormat::PATTERN && tokens.size() != 3) {
            throw std::invalid_argument("Bad Matrix: ill-shaped value line");
        }

        auto row = parse_int<CoordType>(tokens.pop());
        auto col = parse_int<CoordType>(tokens.pop());
        auto value = header.value_type == ValueFormat::PATTERN ? 1 : parse_num<ValueType>(tokens.pop());

        if (row < 1 || row > header.num_rows) {
            throw std::invalid_argument("Bad Matrix: row out of bounds");
        }

        if (col < 1 || col > header.num_cols) {
            throw std::invalid_argument("Bad Matrix: col out of bounds");
        }

        // Fix the 1-indexing
        row--;
        col--;

        nonzeros.push_back(Nonzero<CoordType,ValueType>{row, col, value});
        if (header.symmetry == SymmetryType::SYMMETRIC && row != col) {
            nonzeros.push_back(Nonzero<CoordType,ValueType>{col, row, value});
        }
    }

    // nonzeros is now a COO representation of the matrix
    // the remaining code converts it to CSR

    std::sort(nonzeros.begin(), nonzeros.end(), 
            [](const NonzeroType& a, const NonzeroType& b) {
        return std::tie(a.row, a.col) < std::tie(b.row, b.col);
    });

    std::vector<CoordType> row_offsets;
    std::vector<CoordType> col_indices;
    std::vector<ValueType> values;
    row_offsets.push_back(0);

    CoordType current_row = 0;

    for (const auto& nz : nonzeros) {
        while (current_row < nz.row) {
            row_offsets.push_back(static_cast<CoordType>(col_indices.size()));
            ++current_row;
        }
        col_indices.push_back(nz.col);
        values.push_back(nz.value);
    }

    while (current_row < header.num_rows) {
        row_offsets.push_back(static_cast<CoordType>(col_indices.size()));
        current_row++;
    }

    return CSRMatrix<CoordType, ValueType>{
        header.num_rows,
        header.num_cols,
        static_cast<CoordType>(values.size()),
        std::move(row_offsets),
        std::move(col_indices),
        std::move(values)
    };
}

///////////////////////////////////////////////////////////////////////////////

template<typename CoordType, typename ValueType>
CSCMatrix<CoordType,ValueType> read_csc(const char* filename) {

    static_assert(std::is_integral<CoordType>::value, "CoordType must be integral");
    static_assert(std::is_arithmetic<ValueType>::value, "ValueType must be arithmetic");

    std::ifstream infile(filename);
    if (!infile.is_open()) {
        throw new std::invalid_argument("Could not open file for reading");
    }
    
    auto header = read_header<CoordType>(infile);

    // Read in and populate the non-zeros in "COO format"

    using NonzeroType = Nonzero<CoordType,ValueType>;

    std::vector<NonzeroType> nonzeros;
    for (CoordType i = 0; i < header.num_nonzeros; i++) {
        std::string line;
        std::getline(infile, line);
        auto tokens = Tokens(line, ' ');

        if (header.value_type == ValueFormat::PATTERN && tokens.size() != 2) {
            throw std::invalid_argument("Bad Matrix: ill-shaped pattern line");
        } else if (header.value_type != ValueFormat::PATTERN && tokens.size() != 3) {
            throw std::invalid_argument("Bad Matrix: ill-shaped value line");
        }

        auto row = parse_int<CoordType>(tokens.pop());
        auto col = parse_int<CoordType>(tokens.pop());
        auto value = header.value_type == ValueFormat::PATTERN ? 1 : parse_num<ValueType>(tokens.pop());

        if (row < 1 || row > header.num_rows) {
            throw std::invalid_argument("Bad Matrix: row out of bounds");
        }

        if (col < 1 || col > header.num_cols) {
            throw std::invalid_argument("Bad Matrix: col out of bounds");
        }

        // Fix the 1-indexing
        row--;
        col--;

        nonzeros.push_back(Nonzero<CoordType,ValueType>{row, col, value});
        if (header.symmetry == SymmetryType::SYMMETRIC && row != col) {
            nonzeros.push_back(Nonzero<CoordType,ValueType>{col, row, value});
        }
    }

    // nonzeros is now a COO representation of the matrix
    // the remaining code converts it to CSR

    std::sort(nonzeros.begin(), nonzeros.end(), 
            [](const NonzeroType& a, const
                NonzeroType& b) {
        return std::tie(a.col, a.row) < std::tie(b.col, b.row);
    });

    std::vector<CoordType> col_offsets;
    std::vector<CoordType> row_indices;
    std::vector<ValueType> values;
    col_offsets.push_back(0);
    
    CoordType current_col = 0;

    for (const auto& nz : nonzeros) {
        while (current_col < nz.col) {
            col_offsets.push_back(static_cast<CoordType>(row_indices.size()));
            ++current_col;
        }
        row_indices.push_back(nz.row);
        values.push_back(nz.value);
    }

    while (current_col < header.num_cols) {
        col_offsets.push_back(static_cast<CoordType>(row_indices.size()));
        current_col++;
    }

    return CSCMatrix<CoordType, ValueType>{
        header.num_rows,
        header.num_cols,
        static_cast<CoordType>(values.size()),
        std::move(col_offsets),
        std::move(row_indices),
        std::move(values)
    };
}

} // namespace MatrixMarket