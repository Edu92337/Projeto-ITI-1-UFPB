#include <iostream>
#include <vector>
#include<set>
#include<map>
#include<algorithm>
#include<bitset>
using namespace std;

class Codificador_aritmetico{
    public:
        
        vector<pair<string,double>> gera_mapa(string& palavra){
            vector<pair<string,double>>mapa;
            set<char> vistos;
            double n = palavra.size();
            for(char c : palavra){
                if(vistos.count(c)) continue;
                vistos.insert(c);
                double q = count(palavra.begin(),palavra.end(),c);
                mapa.push_back({string(1,c),q/n});
            }
            return mapa;
        }
        // para usar no ppm
        pair<double,double> encode_byte(string& atual,vector<pair<string,double>>&mapa, double& low, double& high){
            double range = high - low;
            double p0 = low;
            for(auto [s,p] : mapa){
                if(s == atual){
                    high = p0 + range * p;
                    low  = p0;
                    return {low, high};
                }else{
                    p0 += range * p;
                }
            }
        }
        // para usar no codificador semiadaptativo
        double encode_string(string& palavra){
            double low = 0.0, high = 1.0;
            vector<pair<string,double>>mapa = gera_mapa(palavra);
            for(int i = 0;i<palavra.size();i++){
                string atual = string(1,palavra[i]);
               encode_byte(atual, mapa, low, high);
                
            }
            return valor_final(low,high);
        }
        double valor_final(double low, double high){
            string low_s = to_string(low);
            string high_s = to_string(high);
            string final = "";
            for(int i = 0;i<low_s.size();i++){
                if(low_s[i] == high_s[i])final += low_s[i];
                else{
                    final += high_s[i];
                    break;
                }
            }
            return stod(final);
        }
        
};
