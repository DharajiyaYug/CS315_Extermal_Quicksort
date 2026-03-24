#include<bits/stdc++.h>
#include<fstream>
using namespace std;
// Global variables 
int num_keys , size_key , block_size , mem_size , keys_per_block;

int call_id = 0;
int tot_partitions = 0;
int tot_seeks = 0;
int tot_transfers = 0;

unordered_map<int, streampos> cost_pos , sorted_pos;

ostream* pout = &cout;
ostream* cout_cost = &cout;
ostream* sout = &cout;

void usage(){
        cout << "Usage : ./program_name input-file.txt total_keys size_of_key disk_block_size size_memory_in_blocks" << endl;
        exit(0);
}

void partition_runs(vector<vector<int> >& Database ,int n , vector<int>& memory , vector<vector<int> >& left_partition ,int& l_cnt ,
                vector<vector<int> >& right_partition , int& r_cnt ,vector<vector<int> >& pivots , int&p_cnt ,
                int& partition_write_seeks , int& partition_write_transfers , int& partition_read_seeks , int& partition_read_transfers){
         
         int blocks = Database.size();
         int pivot;
         bool pivot_set = false;

         int keys_read = 0;
         int blocks_read = 0;
         int idx = 0;
         int cnt = 0;

         int left_idx = 0;
         int right_idx = 0;
        
         int left_cnt = 0;
         int right_cnt = 0;
         // read m - 3 blocks at a time and keep partitioning using 2 blocks for less and larger and 1 block for persisting the pivot in memory
         while(blocks_read < blocks && keys_read < n){
                // read sub phase
                partition_read_seeks++;
                for(int i = 0 ; i < mem_size - 3 ; i++){
                        partition_read_transfers++;
                        for(int j = 0 ; j < Database[blocks_read].size() ; j++){
                                memory[idx++] = Database[blocks_read][j];
                                keys_read++;
                                if(keys_read == n) break;
                        }
                        blocks_read++;
                        if(blocks_read == blocks) break;
                        if(keys_read == n) break;
                }
                cnt = idx;
                idx = 0;
                

                if(!pivot_set){
                        pivot_set = true;
                        pivot = memory[0];
                        memory.back() = pivot;
                }

                
                // write sub phase
                for(int i = 0 ; i < cnt ; i++){
                        if(memory[i] < memory.back()){
                             l_cnt++;
                             memory[(mem_size-3)*keys_per_block + left_idx] = memory[i];
                             if(++left_idx == keys_per_block){
                                        left_idx = 0;
                                        partition_write_seeks++;
                                        partition_write_transfers++;
                                        left_partition.push_back(
                                                vector<int>(
                                                        memory.begin() + (mem_size-3)*keys_per_block, 
                                                        memory.begin() + (mem_size-2)*keys_per_block
                                                )
                                        );
                             }
                        }
                        else if(memory[i] == memory.back()){
                                p_cnt++;
                        }
                        else{
                             r_cnt++;
                             memory[(mem_size-2)*keys_per_block + right_idx] = memory[i];
                             if(++right_idx == keys_per_block){
                                        right_idx = 0;
                                        partition_write_seeks++;
                                        partition_write_transfers++;
                                        right_partition.push_back(
                                                vector<int>(
                                                        memory.begin() + (mem_size-2)*keys_per_block, 
                                                        memory.begin() +(mem_size-1)*keys_per_block
                                                )
                                        );
                                } 
                        }
                }
         }
         if(left_idx) {
                partition_write_seeks++;
                partition_write_transfers++;
                left_partition.push_back(
                        vector<int>(
                                memory.begin() + (mem_size - 3)*keys_per_block , 
                                memory.begin() + (mem_size-3)*keys_per_block + left_idx
                        )
                );
                left_idx = 0;
         }
         if(right_idx){
                partition_write_seeks++;
                partition_write_transfers++;
                right_partition.push_back(
                        vector<int>(
                                memory.begin() + (mem_size - 2)*keys_per_block , 
                                memory.begin() + (mem_size-2)*keys_per_block + right_idx
                        )
                );
                right_idx = 0;
         }

         // Writing pivots to the pivot file
         int pivot_blocks = p_cnt / keys_per_block + ((p_cnt % keys_per_block) != 0);
         int p_left = p_cnt;
         while(pivot_blocks >= mem_size){
                // fill memory with pivots
                for(int i = 0 ; i < memory.size() ; i++){
                        memory[i] = pivot;       
                }
                partition_write_seeks++;
                for(int i = 0 ; i < mem_size ; i++){
                        partition_write_transfers++;
                        pivots.push_back(
                                vector<int>(
                                        memory.begin() + i*keys_per_block, 
                                        memory.begin() + (i+1)*keys_per_block 
                                )
                        );
                }
                pivot_blocks -= mem_size;
                p_left -= memory.size();
         }
         if(pivot_blocks){
                for(int i = 0 ; i < p_left ; i++){
                        memory[i] = pivot;
                }
                partition_write_seeks++;
                for(int i = 0 ; i < pivot_blocks ; i++){
                        partition_write_transfers++;
                        int l = i*keys_per_block;
                        int r = (i == pivot_blocks - 1) ? p_left : (i+1)*keys_per_block;
                        pivots.push_back(
                                vector<int>(
                                        memory.begin() + l , 
                                        memory.begin() + r
                                )
                        );
                }
         }
}

void process_partition( vector<vector<int> >& partition,int total_cnt,vector<int>& memory,vector<vector<int> >& Database,
                        int& curr_blk, int& curr_idx,int& read_seeks,int& read_transfers,int& write_seeks,int& write_transfers){
    int idx = 0;
    int cnt = 0;
    int read_left = total_cnt;
    int write_left = total_cnt;
    int blk = 0;

    while(blk < partition.size()){
        read_seeks++;

        // read into memory
        for(int i = 0; i < mem_size; i++){
            read_transfers++;

            for(int j = 0; j < keys_per_block; j++){
                memory[idx++] = partition[blk][j];
                read_left--;
                if(!read_left) break;
            }

            blk++;
            if(blk == partition.size()) break;
        }

        cnt = idx / keys_per_block + ((idx % keys_per_block) != 0);

        // write to database
        write_seeks++;
        for(int i = 0; i < cnt; i++){
//            write_transfers++;

            for(int j = 0; j < keys_per_block; j++){
                Database[curr_blk][curr_idx++] = memory[i*keys_per_block + j];

                if(curr_idx == keys_per_block){
                    write_transfers++;
                    curr_idx = 0;
                    curr_blk++;
                }

                write_left--;
                if(!write_left) break;
            }
        }

        idx = 0;
    }
}

void merge_runs(vector<vector<int> >& Database , vector<int>& memory , vector<vector<int> >& left_partition ,int l_cnt ,
                                vector<vector<int> >& right_partition , int r_cnt, vector<vector<int> >& pivots , int p_cnt , 
                                int& merge_read_seeks , int& merge_read_transfers , int& merge_write_seeks , int& merge_write_transfers){
        int curr_blk = 0;
        int curr_idx = 0;
        
        process_partition(left_partition, l_cnt, memory, Database,
                      curr_blk, curr_idx,
                      merge_read_seeks, merge_read_transfers, merge_write_seeks, merge_write_transfers);

        process_partition(pivots, p_cnt, memory, Database,
                      curr_blk, curr_idx,
                      merge_read_seeks, merge_read_transfers, merge_write_seeks, merge_write_transfers);

        process_partition(right_partition, r_cnt, memory, Database,
                      curr_blk, curr_idx,
                      merge_read_seeks, merge_read_transfers, merge_write_seeks, merge_write_transfers);
        if(curr_idx){
                merge_write_transfers++;
                curr_idx = 0;
                curr_blk++;
        }
}
void base_case(vector<vector<int> >& Database , int n , vector<int>& memory , int& partition_read_seeks , int& partition_read_transfers , int& merge_write_seeks , int& merge_write_transfers){
                int blocks = Database.size();
                int idx = 0;
                int blk_id = 0;

                // Empty Partition
                if(n == 0) return;
                
                partition_read_seeks++;
                for(int i = 0 ; i < blocks ; i++){
                        if(idx == n) break;
                        partition_read_transfers++;
                        for(int j = 0 ; j < keys_per_block ; j++){
                                memory[idx++] = Database[i][j];
                                if(idx == n) break;   
                        }                             
                }                                     
                
                sort(memory.begin() , memory.begin() + n);
                        
                idx = 0;
                merge_write_seeks++;
                for(int i = 0 ; i < blocks ; i++){
                        if(idx == n) break;
                        merge_write_transfers++;
                        for(int j = 0 ; j < keys_per_block ; j++){
                                Database[i][j] = memory[idx++];
                                if(idx == n) break;   
                        }                             
                }        
        
        cost_pos[call_id] = (*cout_cost).tellp();

        (*cout_cost) << "\n====================================\n";
        (*cout_cost) << "        COST FOR CALL #" << call_id << "\n";
        (*cout_cost) << "====================================\n";

        (*cout_cost) << "\n[PARTITION PHASE]\n";
        (*cout_cost) << "  Read   : seeks = " << partition_read_seeks
                  << ", transfers = " << partition_read_transfers << "\n";
        (*cout_cost) << "  Write  : seeks = " << 0
                  << ", transfers = " << 0 << "\n";

        (*cout_cost) << "\n[MERGE PHASE]\n";
        (*cout_cost) << "  Read   : seeks = " << 0
                  << ", transfers = " << 0 << "\n";
        (*cout_cost) << "  Write  : seeks = " << merge_write_seeks
                  << ", transfers = " << merge_write_transfers << "\n";

        sorted_pos[call_id] = (*sout).tellp();

        (*sout) << "\n====================================\n";
        (*sout) << "   SORTED OUTPUT AFTER CALL #" << call_id << "\n";
        (*sout) << "====================================\n";

        (*sout) << "Blocks: " << Database.size() << "\n\n";

        for(auto &blk : Database){
            (*sout) << "Block " << setw(3) << blk_id++ << " : ";
            for(int x : blk){
                (*sout) << setw(6) << x;
            }
            (*sout) << "\n";
        }
}
void external_quicksort(vector<vector<int> >& Database , int n ){
 
         int seeks = 0;
         int transfers = 0;
         
         int partition_read_seeks = 0;
         int partition_write_seeks = 0;
         int merge_read_seeks = 0;
         int merge_write_seeks = 0;
         int this_partition = tot_partitions + 1;
         int this_call = call_id + 1;

         int partition_read_transfers = 0;
         int partition_write_transfers = 0;
         int merge_read_transfers = 0;
         int merge_write_transfers = 0;

         int l_cnt = 0;
         int r_cnt = 0;
         int p_cnt = 0;
                
         int blk_id = 0;
         
         vector<int> memory(mem_size * keys_per_block , INT32_MAX);
         vector<vector<int> > left_partition , right_partition , pivots;
        
         call_id++;
         if(Database.size() <= mem_size){
                 //base case 
                 base_case(Database , n , memory , partition_read_seeks , partition_read_transfers , merge_read_seeks , merge_write_transfers);
                 goto out;
         }
         
         
         partition_runs(Database , n , memory , left_partition ,l_cnt , 
                         right_partition , r_cnt , pivots , p_cnt ,partition_write_seeks ,
                         partition_write_transfers , partition_read_seeks , partition_read_transfers);
        (*pout) << "\n====================================\n";
        (*pout) << "        PARTITION PASS #" << this_partition << "\n";
        (*pout) << "====================================\n";

        (*pout) << "\n[LEFT RUNS]\n";
        for(auto &blk : left_partition){
                (*pout) << "  ";
                for(int x : blk) (*pout) << setw(6) << x;
                        (*pout) << "\n";
        }

        (*pout) << "\n[PIVOT RUNS]\n";
        for(auto &blk : pivots){
                (*pout) << "  ";
                for(int x : blk) (*pout) << setw(6) << x;
                        (*pout) << "\n";
        }

        (*pout) << "\n[RIGHT RUNS]\n";
        for(auto &blk : right_partition){
            (*pout) << "  ";
            for(int x : blk) (*pout) << setw(6) << x;
            (*pout) << "\n";
        }
        
         tot_partitions++;

         external_quicksort(left_partition ,l_cnt);
         external_quicksort(right_partition ,r_cnt);

         merge_runs(Database , memory , left_partition , l_cnt , 
                         right_partition , r_cnt ,
                         pivots , p_cnt , merge_read_seeks , merge_read_transfers , merge_write_seeks , merge_write_transfers);
                

        cost_pos[this_call] = (*cout_cost).tellp();

        (*cout_cost) << "\n====================================\n";
        (*cout_cost) << "        COST FOR CALL #" << this_call<< "\n";
        (*cout_cost) << "====================================\n";

        (*cout_cost) << "\n[PARTITION PHASE]\n";
        (*cout_cost) << "  Read   : seeks = " << partition_read_seeks
                  << ", transfers = " << partition_read_transfers << "\n";
        (*cout_cost) << "  Write  : seeks = " << partition_write_seeks
                  << ", transfers = " << partition_write_transfers << "\n";

        (*cout_cost) << "\n[MERGE PHASE]\n";
        (*cout_cost) << "  Read   : seeks = " << merge_read_seeks
                  << ", transfers = " << merge_read_transfers << "\n";
        (*cout_cost) << "  Write  : seeks = " << merge_write_seeks
                  << ", transfers = " << merge_write_transfers << "\n";

        sorted_pos[this_call] = (*sout).tellp();

        (*sout) << "\n====================================\n";
        (*sout) << "   SORTED OUTPUT AFTER CALL #" << this_call << "\n";
        (*sout) << "====================================\n";

        (*sout) << "Blocks: " << Database.size() << "\n\n";

        for(auto &blk : Database){
            (*sout) << "Block " << setw(3) << blk_id++ << " : ";
            for(int x : blk){
                (*sout) << setw(6) << x;
            }
            (*sout) << "\n";
        }

out:
        seeks = partition_read_seeks + partition_write_seeks + merge_read_seeks + merge_write_seeks;
        transfers = partition_read_transfers + partition_write_transfers + merge_write_transfers + merge_read_transfers;

        tot_seeks += seeks;
        tot_transfers += transfers;
}

int main(int argc , char * argv[]){
        if(argc != 6){
                usage();
        }
        
        string input_file = argv[1];
        num_keys = stoi(argv[2]);
        size_key = stoi(argv[3]);
        block_size = stoi(argv[4]);
        mem_size = stoi(argv[5]);
        

        // assuming that keys are not broken across blocks , as in if block size = 10 and key size  = 4 , then in 2 blocks there are 4 keys not 5.
        keys_per_block = block_size / size_key ;
        int num_blocks = num_keys / keys_per_block + ((num_keys % keys_per_block) != 0);
        
        vector<vector<int> > Database(num_blocks , vector<int>(keys_per_block , INT32_MAX));

        int curr_block = 0;
        int curr_count = 0;
        
        ofstream partition_file , cost_file , sort_file;
        
        ifstream fin(input_file);
        if(!fin){
                cout << "Error opening file";
                usage();
        }
        
        if(num_keys <= 0 || size_key <= 0 || block_size <= 0 || mem_size <= 0){
                cout << "Error : All inputs should be positive\n";
                usage();
        }
        
        if(block_size < size_key){
                cout << "Error : Block size should be greater than size of key\n";
                usage();      
        }

        if(mem_size - 3 <= 0) {
                cout << "Error : Memory must be atleast 3 blocks for partitioning\n";
                usage();
        }

        
        int key_count = 0;
        string key;
        
        while(getline(fin , key)){
                Database[curr_block][curr_count++] = stoi(key);
                key_count++;                
                if(curr_count == keys_per_block){
                        curr_count = 0;
                        curr_block++;
                }
        } 

        fin.close();
        
        if(key_count != num_keys) {
                cout << "No of keys provided did not match with number of keys in given file" << endl;
                usage();
        } 
        partition_file.open("partition_log.txt");
        cost_file.open("cost_log.txt");
        sort_file.open("sorted_log.txt");

        if(partition_file.is_open()){
                pout = &partition_file;
        }
        if(cost_file.is_open()){
                 cout_cost = &cost_file;
        }
        if(sort_file.is_open()){
                sout = &sort_file;
        }        

        external_quicksort(Database , num_keys);
        
        partition_file.close();
        cost_file.close();
        sort_file.close();

        
        cout << "\n===== PARTITION LOG =====\n";
        ifstream pin("partition_log.txt");
        string line;

        while(getline(pin, line)){
                cout << line << endl;
        }
        pin.close();
        
        cout << "\n===== COST LOG =====\n";
        ifstream cinfile("cost_log.txt");

        for(int i = 1; i <= call_id; i++){
                cinfile.clear();
                cinfile.seekg(cost_pos[i]);

                string line;
                while(getline(cinfile , line)){
                        if(line.find("COST FOR CALL #") != string::npos && line.find('#' + to_string(i)) == string::npos){
                                break;
                        }
                        cout << line << endl;
                }
        }
        cinfile.close();
        
        cout << "\n=========== SORTED OUTPUT (ORDERED) ===========\n";

        ifstream sfin("sorted_log.txt");

        for(int i = 1; i <= call_id; i++){
            sfin.clear();
            sfin.seekg(sorted_pos[i]);

            string line;
            while(getline(sfin, line)){
                if(line.find("SORTED OUTPUT AFTER CALL #") != string::npos &&
                   line.find("#" + to_string(i)) == string::npos){
                        break;
                }
                cout << line << endl;
            }
        }
        sfin.close();

        cout << "\nTotal partition passes: " << tot_partitions << endl;
        cout << "Total seeks: " << tot_seeks << endl;
        cout << "Total transfers: " << tot_transfers << endl;
        
        return 0;
}
