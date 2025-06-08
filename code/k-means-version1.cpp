#include <iostream>
#include <vector>
#include <fstream>
#include <cmath>
#include <string>


struct Point {
    double x, y;

    //Manhattan distance between two points
    double MDistance(const Point& other) const {
        return std::abs (x- other.x) + std::abs(y - other.y);
    }

    //euclidian distance between two points
    double EDistance(const Point& other) const{
        return std::sqrt(std::pow(x-other.x,2) + std::pow(y-other.y,2));
    }

    std::string Point_To_String() const{
        return "(" + std::to_string(x) + "," + std::to_string(y) + ")";
    }
};

std::vector<std::vector<Point>> Clustering(const std::vector<Point>& centroid, const std::vector<Point>& points,int k){
    std::vector<std::vector<Point>> clusters(k);

    for(int i = 0; i < points.size(); i++){
        //distances between centroids
        std::vector<double> distance;

        for(int j = 0; j < k; j++){
            distance.push_back(centroid[j].MDistance(points[i]));
        }
        
        //determines smallest distance between centroids
        int smallest = 0;
        for(int l = 1; l < k; l++){
            if(distance[smallest] > distance[l]){
                smallest = l;
            }
        }
        
        //appends point to associated centroid effectively "clustering"
        clusters[smallest].push_back(points[i]);
    }

    return clusters;
}

std::vector<Point> New_Centroid(const std::vector<Point>& centroid, const std::vector<std::vector<Point>>& clusters){
    std::vector<Point> centre = centroid;

    //recaculate centroids
    for(int i = 0; i < centroid.size(); i++){
        Point NewCentroid;

        double ClusterSize = double(clusters[i].size());

        double sumx = 0, sumy = 0;
        for(int j = 0; j < ClusterSize; j++){
            sumx += clusters[i][j].x;
            sumy += clusters[i][j].y;
        }
        
        if(ClusterSize == 0){
            NewCentroid = centre[i];
        }
        else{
            NewCentroid.x = sumx/(ClusterSize);
            NewCentroid.y = sumy/(ClusterSize);
        }

        centre[i] = NewCentroid;
    }

    return centre;
}


void write_output_csv(
    const std::string &filename,
    const std::vector<Point> &centroids,
    const std::vector<std::vector<Point>> &clusters)
{
    std::ofstream out(filename);
    // header
    out << "type,cluster,x,y\n";
    
    // 1) all the points, tagged by which cluster they belong to
    for (int ci = 0; ci < (int)clusters.size(); ++ci) {
        for (auto &p : clusters[ci]) {
            out << "point," 
                << ci  << "," 
                << p.x << "," 
                << p.y << "\n";
        }
    }
    
    // 2) all the centroids
    for (int ci = 0; ci < (int)centroids.size(); ++ci) {
        auto &c = centroids[ci];
        out << "centroid,"
            << ci  << ","
            << c.x << ","
            << c.y << "\n";
    }
    
    out.close();
}


int main(){
    std::ifstream infile;

    infile.open("data.txt");

    if(!infile.is_open()){
        return EXIT_FAILURE;
    }

    int k = 3; // number of clusters
    std::vector<Point> centroid(k);
    std::vector<std::vector<Point>> clusters(k); //initializes vector of size k with empty clusters

    //set centroid coordinates at clusters[i] and centroid[i] are complementary

    //set centroid
    for(int i = 0; i < k; i++){
        std::cin >> centroid[i].x >> centroid[i].y;
    }

    //store all data points
    std::vector<Point> points;
    Point p;
    char comma;
    while(infile >> p.x >> comma >> p.y){
        points.push_back(p);
    }

    infile.close();

    int max_iter = 300; //maximum number of iterations
    int iter = 0; //current iteration
    bool completion = false;
    while((iter < max_iter) && (completion == false)){
        //establish clusters
        clusters = Clustering(centroid,points,k);

        //recalculate centroids
        std::vector<Point> Newcentroid = New_Centroid(centroid,clusters);

        //convergence check
        completion = true;
        for(int i = 0; i < k; i++){
            if((centroid[i].x != Newcentroid[i].x) || (centroid[i].y != Newcentroid[i].y)){
                completion = false;
                break;
            }
        }

        centroid = Newcentroid;
        iter++;
    }


    std::cout << "final outcome: " << std::endl;
    for(int i = 0; i < k; i++){
        std::cout << "C" << i << " " << centroid[i].Point_To_String() << ": ";
        for(int j = 0; j < clusters[i].size(); j++){
            std::cout << clusters[i][j].Point_To_String() << ", ";
        }
        std::cout << std::endl;
    }

    write_output_csv("kmeans_out.csv", centroid, clusters);

}