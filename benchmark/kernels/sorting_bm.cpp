#include "kbm_utils.hpp"

void sorting_kernel(size_t lower, size_t upper)
{
    const size_t scale = 50;
    const size_t offset = 1;
    const size_t range = upper - lower;

    float **vecs = new float *[range];
    for (size_t iteration = 0; iteration < range; iteration++)
    {
        const size_t n = offset + (iteration + lower) * scale;
        vecs[iteration] = new float[n];
    }

    // Main computation loop
    for (size_t iteration = 0; iteration < range; iteration++)
    {
        const size_t n = offset + (iteration + lower) * scale;

        // Generate random data
        int seed = static_cast<int>(iteration + lower);
        for (size_t i = 0; i < n; i++)
        {
            seed = seed * 0x343fd + 0x269EC3; // a=214013, b=2531011
            float rand_val = (seed / 65536) & 0x7FFF;
            vecs[iteration][i] = static_cast<float>(rand_val) / static_cast<float>(RAND_MAX);
        }

        // Bubble sort
        for (long i = 0; i < static_cast<long>(n) - 1; i++)
        {
            // Last i elements are already in place
            for (long j = 0; j < static_cast<long>(n) - i - 1; j++)
            {
                if (vecs[iteration][j] > vecs[iteration][j + 1])
                {
                    const float temp = vecs[iteration][j];
                    vecs[iteration][j] = vecs[iteration][j + 1];
                    vecs[iteration][j + 1] = temp;
                }
            }
        }
    }

    for (size_t iteration = 0; iteration < range; iteration++)
    {
        delete[] vecs[iteration];
    }
    delete[] vecs;
}

int main(int argc, char *argv[])
{
    const double start_time = get_time_stamp();
    sorting_kernel(0, 200);
    print_elapsed(argv[0], start_time);
}