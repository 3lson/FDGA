#include <stdio.h>

struct Point {
    float x, y;
};

float fabsf(float x) {
    return x < 0 ? -x : x;
}

int main() {
    int MAX_K = 10;
    int MAX_POINTS = 10000;
    float EPS = 0.000001;
    int k = 3;
    struct Point centroids[MAX_K];
    struct Point points[MAX_POINTS];
    struct Point old_centroids[MAX_K];
    int num_points = 1000;

    // Initial centroids
    for (int i = 0; i < 2; i++) {
        centroids[i].x = 3.2 + 2 * i;
        centroids[i].y = 2.1 + 2 * i;
    }
    centroids[2].x = 4.6;
    centroids[2].y = 3.5;

    // Read points
    FILE *fin = fopen("points.txt", "r");
    if (!fin) {
        printf("Error opening points.txt for reading.\n");
        return 1;
    }

    num_points = 0;
    while (num_points < MAX_POINTS && fscanf(fin, "%f %f", &points[num_points].x, &points[num_points].y) == 2) {
        num_points++;
    }
    fclose(fin);

    struct Point clusters[MAX_K][MAX_POINTS];
    int cluster_sizes[MAX_K];

    int max_iter = 100000;
    for (int cycle = 0; cycle < max_iter; cycle++) {

        for (int i = 0; i < k; i++) {
            old_centroids[i] = centroids[i];
        }

        // Reset clusters
        for (int i = 0; i < k; i++) {
            cluster_sizes[i] = 0;
        }

        // Assign points to nearest centroid
        for (int i = 0; i < num_points; i++) {
            float min_dist = fabsf(centroids[0].x - points[i].x) + fabsf(centroids[0].y - points[i].y);
            int best = 0;

            for (int j = 1; j < k; j++) {
                float d = fabsf(centroids[j].x - points[i].x) + fabsf(centroids[j].y - points[i].y);
                if (d < min_dist) {
                    min_dist = d;
                    best = j;
                }
            }
            int old_best = cluster_sizes[best];
            clusters[best][old_best] = points[i];
            cluster_sizes[best] = old_best + 1;
        }
        // Output this iteration to file
        FILE *fout = fopen("output.txt", cycle == 0 ? "w" : "a");
        if (!fout) {
            printf("Error opening output.txt for writing.\n");
            return 1;
        }

        fprintf(fout, "ITERATION %d\n", cycle);
        for (int i = 0; i < k; i++) {
            fprintf(fout, "C%d (%.2f, %.2f): ", i, centroids[i].x, centroids[i].y);
            for (int j = 0; j < cluster_sizes[i]; j++) {
                fprintf(fout, "(%.2f, %.2f) ", clusters[i][j].x, clusters[i][j].y);
            }
            fprintf(fout, "\n");
        }
        fprintf(fout, "END\n\n");
        fclose(fout);

        // Recalculate centroids
        for (int i = 0; i < k; i++) {
            float sum_x = 0, sum_y = 0;
            int size = cluster_sizes[i];
            for (int j = 0; j < size; j++) {
                sum_x += clusters[i][j].x;
                sum_y += clusters[i][j].y;
            }

            if (size > 0) {
                centroids[i].x = sum_x / size;
                centroids[i].y = sum_y / size;
            }
        }

        int done = 1;
        for (int i = 0; i < k; i++) {
            if (!((fabsf(centroids[i].x - old_centroids[i].x) < EPS) &&
                  (fabsf(centroids[i].y - old_centroids[i].y) < EPS))) {
                done = 0;
                break;
            }
        }
        if (done) break;
    }

    return 0;
}
