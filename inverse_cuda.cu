#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <cuda_runtime.h>
#include <cmath>

#define IDX2C(i,j,ld) (((j)*(ld))+(i))

__global__ void normalize_row(double* mat, int n, int row, double pivot) {
    int idx = threadIdx.x + blockDim.x * blockIdx.x;
    if (idx < 2 * n)
        mat[IDX2C(row, idx, n)] /= pivot;
}

__global__ void eliminate_rows(double* mat, int n, int row) {
    int i = blockIdx.x;
    int j = threadIdx.x;

    if (i == row || i >= n || j >= 2 * n) return;

    __shared__ double pivot_row[2048];
    pivot_row[j] = mat[IDX2C(row, j, n)];
    __syncthreads();

    double factor = mat[IDX2C(i, row, n)];
    mat[IDX2C(i, j, n)] -= factor * pivot_row[j];
}

void generateMatrix(double* mat, int n) {
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            mat[IDX2C(i, j, n)] = (i == j) ? 1.0 + rand() % 5 : rand() % 3 - 1;

    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            mat[IDX2C(i, j + n, n)] = (i == j) ? 1.0 : 0.0;
}

void printMatrix(double* mat, int n, const std::string& title) {
    std::cout << "\n" << title << ":\n";
    for (int i = 0; i < n; ++i) {
        for (int j = n; j < 2 * n; ++j)
            std::cout << std::setw(10) << std::fixed << std::setprecision(4) << mat[IDX2C(i, j, n)] << " ";
        std::cout << "\n";
    }
}

int main() {
    int n;
    std::cout << "Введите размерность матрицы: ";
    std::cin >> n;

    size_t size = n * 2 * n * sizeof(double);
    double* h_mat = (double*)malloc(size);
    generateMatrix(h_mat, n);

    double* d_mat;
    cudaMalloc((void**)&d_mat, size);
    cudaMemcpy(d_mat, h_mat, size, cudaMemcpyHostToDevice);

    cudaEvent_t start, stop;
    cudaEventCreate(&start);
    cudaEventCreate(&stop);
    cudaEventRecord(start);

    for (int i = 0; i < n; ++i) {
        cudaDeviceSynchronize();
        double pivot;
        cudaMemcpy(&pivot, &d_mat[IDX2C(i, i, n)], sizeof(double), cudaMemcpyDeviceToHost);

        if (fabs(pivot) < 1e-6) {
            std::cerr << "Матрица вырождена (pivot = " << pivot << ").\n";
            cudaFree(d_mat);
            free(h_mat);
            return -1;
        }

        normalize_row<<<(2 * n + 255) / 256, 256>>>(d_mat, n, i, pivot);
        eliminate_rows<<<n, 2 * n>>>(d_mat, n, i);
    }

    cudaEventRecord(stop);
    cudaEventSynchronize(stop);

    float milliseconds = 0;
    cudaEventElapsedTime(&milliseconds, start, stop);

    cudaMemcpy(h_mat, d_mat, size, cudaMemcpyDeviceToHost);
    if (n <= 10)
        printMatrix(h_mat, n, "Обратная матрица");

    std::cout << "Время выполнения: " << milliseconds << " мс\n";

    cudaFree(d_mat);
    free(h_mat);
    return 0;
}
