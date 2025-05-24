struct Point {
    float x, y;
};

float fabsf(float x) {
    return x < 0 ? -x : x;
}

float f() {
    int MAX_K = 3;
    int MAX_POINTS = 50;
    float EPS = 0.000001;
    int k = 3;
    struct Point centroids[MAX_K];
    struct Point points[MAX_POINTS];
    struct Point old_centroids[MAX_K];
    int num_points = 9;
    float sum = 0;

    // geberates initial centroids  need to change that!!!!!!!!!!!!!
    for (int i = 0; i < k; i++) {
        centroids[i].x = 1+ 2*i;
        centroids[i].y = 2+ 2*i;
    }
    // generates poitns             need to change that !!!!!!!!!!!!!!
    for (int i = 0; i < num_points; i++) {
        points[i].x = 1 + 2*i;
        points[i].y = 2 + 2*i;
    }

    struct Point clusters[MAX_K][MAX_POINTS];
    int cluster_sizes[MAX_K];

    int max_iter = 100;
    for (int cycle = 0; cycle < max_iter; cycle++) {

        for (int i = 0; i < k; i++) {
            old_centroids[i] = centroids[i];
        }

        // updated clustering functions
        for (int i = 0; i < k; i++) {
            cluster_sizes[i] = 0;
        }

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
        //updated centroids function
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
            if (!((fabsf(centroids[i].x - old_centroids[i].x) < EPS) && (fabsf(centroids[i].y - old_centroids[i].y) < EPS))){
                done = 0;
                break;
            }
        }
        if (done) break;
    }
        for (int i = 0; i < k; i++) {
            sum += centroids[i].x + centroids[i].y;
        }
    return sum;
}
