#include <stdio.h>
// This can be removed during compilation

//Replaced with abs instr
float fabsf(float x) {
    return x < 0 ? -x : x;
}
//Replaced

float OUT_centroids_x[3];
float OUT_centroids_y[3];

float OUT_clusters_x[3][20];
float OUT_clusters_y[3][20];
int OUT_cluster_sizes[3];


int main(){
    float centroids_x[3];
    float centroids_y[3];
    float old_centroids_x[3];
    float old_centroids_y[3];
    float clusters_x[3][20];
    float clusters_y[3][20];
    int cluster_sizes[3];
    int max_iter =10000;
    int num_points =20;
    int cycle;
    int j;
    int i;
    int done =1;
    int best;

    float points_x[20];
    float points_y[20];

    FILE *fin = fopen("out/points.txt", "r");
    num_points = 0;
    while (fscanf(fin, "%f %f", &points_x[num_points], &points_y[num_points]) == 2) {
        num_points++;
    }
    fclose(fin);

    FILE *fin2 = fopen("out/clicked_points.txt", "r");
    int num_clusters = 0;
    while (num_clusters < 3 && fscanf(fin, "%f %f", &centroids_x[num_clusters], &centroids_y[num_clusters]) == 2) {
        num_clusters++;
    }
    fclose(fin2);

    for(cycle=0; cycle<max_iter; cycle++){
        done = 1;
        for(j=0; j<3; j++){
            old_centroids_x[j] = centroids_x[j];
            old_centroids_y[j] = centroids_y[j];
            cluster_sizes[j] = 0;
        }
        for(i=0; i<num_points; i++){
            float min_dist = fabsf(centroids_x[0] - points_x[i]) + fabsf(centroids_y[0] - points_y[i]);
            best = 0;
            for (j = 1; j < 3; j++) {
                float d = fabsf(centroids_x[j] - points_x[i]) + fabsf(centroids_y[j] - points_y[i]);
                if (d<min_dist){
                    min_dist = d;
                    best = j;
                }
            }
            clusters_x[best][cluster_sizes[best]] = points_x[i];
            clusters_y[best][cluster_sizes[best]] = points_y[i];
            cluster_sizes[best]++;
        }

        for (i = 0; i < 3; i++) {
            float sum_x = 0, sum_y = 0;
            int size = cluster_sizes[i];
            for (j = 0; j < size; j++) {
                sum_x += clusters_x[i][j];
                sum_y += clusters_y[i][j];
            }
            if (size > 0) {
                centroids_x[i] = sum_x / size;
                centroids_y[i] = sum_y / size;
            }
        }
        for (i = 0; i < 3; i++) {
            float side1 = fabsf(centroids_x[i] - old_centroids_x[i]);
            float side2 = fabsf(centroids_y[i] - old_centroids_y[i]);
            if (!(side1 < 0.000001 && side2 < 0.000001)) {
                done = 0;
                break;
            }
        }
        if (done) {
            break;
        }
    }
    // Output this iteration to file
    FILE *fout = fopen("out/output.txt","w");
    if (!fout) {
        printf("Error opening output.txt for writing.");
        return 1;
    }
    fprintf(fout, "ITERATION %d", cycle);
    for (i = 0; i < 3; i++) {
        OUT_centroids_x[i] = centroids_x[i];
        OUT_centroids_y[i] = centroids_y[i];
        OUT_cluster_sizes[i] = cluster_sizes[i];
        fprintf(fout, "\n");
        fprintf(fout, "C%d (%.2f, %.2f): ", i, OUT_centroids_x[i], OUT_centroids_y[i]);
        for (j = 0; j < cluster_sizes[i]; j++) {
            OUT_clusters_x[i][j] = clusters_x[i][j];
            OUT_clusters_y[i][j] = clusters_y[i][j];
            fprintf(fout, "(%.2f, %.2f) ", OUT_clusters_x[i][j], OUT_clusters_y[i][j]);
        }
    }
    fprintf(fout, "\n");
    fprintf(fout, "END\n\n");
    fclose(fout);
    
    return 5;
}
