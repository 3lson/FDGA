#include <iostream>

int main(){
    int sum[5] = {1,2,3,4,5};

    for(int i = 0; i < 3; i++){
        for(int j = 0; j < 5; j++){
            if((j + (1 << i)) < 5){
                sum[j] = sum[j] + sum[j + (1 << i)];
            }
            else { 
                sum[j] = sum[j];
            }
        }
    }

    std::cout << sum[0] << std::endl;
}