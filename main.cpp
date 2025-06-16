#include <mpi.h>
#include <vector>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstdlib>

using namespace std;

void printMatrix(const vector<vector<double>>& mat, const string& name) {
    cout << "\n" << name << ":\n";
    for (const auto& row : mat) {
        for (double val : row)
            cout << setw(10) << fixed << setprecision(4) << val << " ";
        cout << "\n";
    }
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int n;

    if (rank == 0) {
        cout << "Enter matrix size: ";
        cin >> n;
    }

    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);

    int rows_per_proc = n / size;
    int extra_rows = n % size;
    int my_rows = rows_per_proc + (rank < extra_rows ? 1 : 0);

    vector<vector<double>> local_aug(my_rows, vector<double>(2 * n));
    vector<vector<double>> full_matrix;

    if (rank == 0) {
        srand(time(0));
        full_matrix.assign(n, vector<double>(n));
        for (auto& row : full_matrix)
            for (auto& el : row)
                el = rand() % 100 - 50;

        for (int i = 0, dest = 0, offset = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j)
                full_matrix[i].push_back((i == j) ? 1.0 : 0.0);
        }
    }

    vector<double> sendbuf, recvbuf(my_rows * 2 * n);
    if (rank == 0) {
        sendbuf.resize(n * 2 * n);
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < 2 * n; ++j)
                sendbuf[i * 2 * n + j] = full_matrix[i][j];
    }

    vector<int> sendcounts(size), displs(size);
    int offset = 0;
    for (int i = 0; i < size; ++i) {
        int count = (n / size + (i < extra_rows ? 1 : 0)) * 2 * n;
        sendcounts[i] = count;
        displs[i] = offset;
        offset += count;
    }

    MPI_Scatterv(sendbuf.data(), sendcounts.data(), displs.data(), MPI_DOUBLE,
                 recvbuf.data(), recvbuf.size(), MPI_DOUBLE,
                 0, MPI_COMM_WORLD);

    for (int i = 0; i < my_rows; ++i)
        for (int j = 0; j < 2 * n; ++j)
            local_aug[i][j] = recvbuf[i * 2 * n + j];

    double start = MPI_Wtime();

    for (int i = 0; i < n; ++i) {
        int owner = 0, row_index = i;
        int rows = 0, offset = 0;

        for (int r = 0; r < size; ++r) {
            int r_rows = n / size + (r < extra_rows ? 1 : 0);
            if (row_index < offset + r_rows) {
                owner = r;
                row_index -= offset;
                break;
            }
            offset += r_rows;
        }

        vector<double> pivot_row(2 * n);

        if (rank == owner)
            pivot_row = local_aug[row_index];

        MPI_Bcast(pivot_row.data(), 2 * n, MPI_DOUBLE, owner, MPI_COMM_WORLD);

        double pivot_val = pivot_row[i];
        if (fabs(pivot_val) < 1e-12) {
            if (rank == 0) cerr << "Matrix is degenerate.\n";
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        for (int r = 0; r < my_rows; ++r) {
            if (local_aug[r][i] == pivot_val && rank == owner && r == row_index)
                continue;

            double factor = local_aug[r][i];
            for (int j = 0; j < 2 * n; ++j)
                local_aug[r][j] -= factor * pivot_row[j];
        }

        if (rank == owner) {
            for (int j = 0; j < 2 * n; ++j)
                pivot_row[j] /= pivot_val;
            local_aug[row_index] = pivot_row;
        }
    }

    for (int i = 0; i < my_rows; ++i)
        for (int j = 0; j < 2 * n; ++j)
            recvbuf[i * 2 * n + j] = local_aug[i][j];
MPI_Gatherv(recvbuf.data(), recvbuf.size(), MPI_DOUBLE,
                sendbuf.data(), sendcounts.data(), displs.data(), MPI_DOUBLE,
                0, MPI_COMM_WORLD);

    double end = MPI_Wtime();

    if (rank == 0) {
        vector<vector<double>> inverse(n, vector<double>(n));
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                inverse[i][j] = sendbuf[i * 2 * n + j + n];

        if (n <= 10)
            printMatrix(inverse, "Inversed matrix");

        cout << "Lead time: " << fixed << setprecision(3)
             << (end - start) * 1000.0 << " ms\n";
    }

    MPI_Finalize();
    return 0;
}
