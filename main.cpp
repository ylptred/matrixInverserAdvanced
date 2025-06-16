#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <cmath>
#include <omp.h>

using namespace std;

void printMatrix(const vector<vector<double>>& matrix, const string& name) {
    int n = matrix.size();
    cout << "\n" << name << ":\n";
    cout << fixed << setprecision(3);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            cout << setw(10) << matrix[i][j] << ((j == n - 1) ? "\n" : " ");
}

vector<vector<double>> generateRandomMatrix(int n) {
    vector<vector<double>> matrix(n, vector<double>(n));
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            matrix[i][j] = ((rand() % 2001) - 1000) / 100.0;
    return matrix;
}

bool invertMatrix(const vector<vector<double>>& input, vector<vector<double>>& inverse) {
    int n = input.size();
    inverse.assign(n, vector<double>(n, 0.0));

    vector<vector<double>> aug(n, vector<double>(2 * n));

    for (int i = 0; i < n; ++i) {
        #pragma GCC ivdep
        for (int j = 0; j < n; ++j)
            aug[i][j] = input[i][j];
        aug[i][n + i] = 1.0;
    }

    for (int i = 0; i < n; ++i) {
        double maxEl = fabs(aug[i][i]);
        int maxRow = i;
        for (int k = i + 1; k < n; ++k) {
            double val = fabs(aug[k][i]);
            if (val > maxEl) {
                maxEl = val;
                maxRow = k;
            }
        }

        if (fabs(maxEl) < 1e-12) {
            cerr << "The matrix is degenerate, there is no inverse.\n";
            return false;
        }

        if (i != maxRow)
            swap(aug[i], aug[maxRow]);

        double pivot = aug[i][i];

        #pragma omp simd
        for (int j = 0; j < 2 * n; ++j)
            aug[i][j] /= pivot;

        for (int k = 0; k < n; ++k) {
            if (k == i) continue;
            double coeff = aug[k][i];

            #pragma omp simd
            for (int j = 0; j < 2 * n; ++j)
                aug[k][j] -= coeff * aug[i][j];
        }
    }

    for (int i = 0; i < n; ++i)
        #pragma omp simd
        for (int j = 0; j < n; ++j)
            inverse[i][j] = aug[i][j + n];

    return true;
}

int main() {
    int n;
    cout << "Enter the dimension of the square matrix: ";
    cin >> n;

    if (n <= 0) {
        cerr << "The dimension must be positive.\n";
        return 1;
    }

    srand(static_cast<unsigned>(time(0)));
    vector<vector<double>> A = generateRandomMatrix(n);
    vector<vector<double>> A_inv;

    if (n <= 10)
        printMatrix(A, "The generated matrix A");

    double start = omp_get_wtime();
    bool success = invertMatrix(A, A_inv);
    double end = omp_get_wtime();

    double elapsed_ms = (end - start) * 1000.0;

    if (success) {
        cout << "\nThe inverse matrix has been successfully found." << endl;
        cout << "Lead time: " << fixed << setprecision(3) << elapsed_ms << " ms." << endl;

        if (n <= 10)
            printMatrix(A_inv, "Inverse matrix A^(-1)");
    } else {
        cout << "The inverse matrix could not be found." << endl;
    }

    return 0;
}
