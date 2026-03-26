#include<bits/stdc++.h>
#include<fstream>
using namespace std;
int global_file_id_counter = 0;

struct DiskFile {
    int file_id;
    vector<vector<int> > blocks;

    // Constructor automatically assigns a unique physical file ID
    DiskFile() {
        file_id = global_file_id_counter++;
    }
};

// Global variables 
int num_keys , key_size , block_size , mem_size , keys_per_block;

int call_id = 0;
int tot_partitions = 0;
int tot_seeks = 0;
int tot_transfers = 0;

int current_head_file_id = -1;
int current_head_block = -1;

#define left_buffer_offset ((mem_size - 3)*keys_per_block)
#define right_buffer_offset ((mem_size - 2)*keys_per_block)
#define pivot_buffer_offset ((mem_size - 1)*keys_per_block)

unordered_map<int, streampos> cost_pos , sorted_pos;

ostream* pout = &cout;
ostream* cout_cost = &cout;
ostream* sout = &cout;

void usage(){
        cout << "Usage : ./program_name input-file.txt total_keys size_of_key disk_block_size size_memory_in_blocks" << endl;
        exit(0);
}

void execute_disk_io(int target_file_id, int target_block_id,
                     int& seeks_counter, int& transfers_counter) {

    // A seek is ONLY charged if we switch files, OR if we jump to a non-contiguous block
    if (current_head_file_id != target_file_id || current_head_block != target_block_id) {
        seeks_counter++;
    }

    // A transfer is always charged when moving a block
    transfers_counter++;

    // Update the physical head position to the NEXT block
    current_head_file_id = target_file_id;
    current_head_block = target_block_id + 1;
}

void read_block_from_disk(const struct DiskFile& disk_file, int block_id,
                          vector<int>& memory, int mem_offset, int keys_to_read,
                          int& seeks_counter , int& transfers_counter) {
    execute_disk_io(disk_file.file_id , block_id  ,seeks_counter , transfers_counter);
    for (int i = 0; i < keys_to_read; i++) {
        memory[mem_offset + i] = disk_file.blocks[block_id][i];
    }
}

void overwrite_block_in_database(struct DiskFile& database, int block_id, 
                                 const vector<int>& memory, int mem_offset, int keys_to_write, 
                                 int& seeks_counter , int& transfers_counter) {
    execute_disk_io(database.file_id , block_id , seeks_counter , transfers_counter);
    for (int i = 0; i < keys_to_write; i++) {
        database.blocks[block_id][i] = memory[mem_offset + i];
    }
}

void append_block_to_partition(struct DiskFile& partition_file, 
                               const vector<int>& memory, int mem_offset, int keys_to_write, 
                               int& seeks_counter , int& transfers_counter) {
    execute_disk_io(partition_file.file_id , partition_file.blocks.size() , seeks_counter , transfers_counter );
    partition_file.blocks.push_back(vector<int>(
        memory.begin() + mem_offset, 
        memory.begin() + mem_offset + keys_to_write
    ));
}

void partition_runs(struct DiskFile& Database ,int n , vector<int>& memory , struct DiskFile & left_partition ,int& l_cnt ,
                struct DiskFile & right_partition , int& r_cnt ,struct DiskFile & pivots , int&p_cnt ,
                int& partition_write_seeks , int& partition_write_transfers , int& partition_read_seeks , int& partition_read_transfers){
         
         int blocks = Database.blocks.size();
         int pivot = 0;
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
                //..partition_read_seeks++;
                for(int i = 0 ; i < mem_size - 3 ; i++){
                        
                        if(blocks_read == blocks || keys_read == n) break;
                        int keys_in_this_block = min((int)Database.blocks[blocks_read].size(), n - keys_read);

                        read_block_from_disk(Database , blocks_read , memory , idx , keys_in_this_block , partition_read_seeks , partition_read_transfers);
                        keys_read += keys_in_this_block;
                        idx += keys_in_this_block;
                        blocks_read++;
                }
                cnt = idx;
                idx = 0;
                

                if(!pivot_set){
                        pivot_set = true;
                        pivot = memory[0];
                        memory[pivot_buffer_offset] = pivot;
                }

                // write sub phase
                for(int i = 0 ; i < cnt ; i++){
                        if(memory[i] < pivot){
                             l_cnt++;
                             memory[left_buffer_offset + left_idx] = memory[i];
                             if(++left_idx == keys_per_block){
                                        append_block_to_partition(left_partition, memory, left_buffer_offset, keys_per_block, 
                                              partition_write_seeks, partition_write_transfers);
                                        left_idx = 0;
                             }
                        }
                        else if(memory[i] == pivot){
                                p_cnt++;
                        }
                        else{
                             r_cnt++;
                             memory[right_buffer_offset + right_idx] = memory[i];
                             if(++right_idx == keys_per_block){
                                     append_block_to_partition(right_partition, memory, right_buffer_offset, keys_per_block, 
                                              partition_write_seeks, partition_write_transfers);   
                                     right_idx = 0;
                                } 
                        }
                }
         }
         
         if(left_idx) {
                 append_block_to_partition(left_partition, memory, left_buffer_offset, left_idx, 
                                  partition_write_seeks, partition_write_transfers);
                 left_idx = 0;
         }
         if(right_idx){
                 append_block_to_partition(right_partition, memory, right_buffer_offset, right_idx, 
                                  partition_write_seeks, partition_write_transfers);
                 right_idx = 0;
         }

         // Writing pivots to the pivot file
         int pivot_blocks = p_cnt / keys_per_block + ((p_cnt % keys_per_block) != 0);
         int p_left = p_cnt;
         
         for(int i = 0 ; i < keys_per_block ; i++){
                memory[pivot_buffer_offset + i] = pivot;       
         }
        
         for (int i = 0; i < pivot_blocks; i++) {
                
                int keys_to_write = (i == pivot_blocks - 1 && p_left % keys_per_block != 0)
                                    ? (p_left % keys_per_block) : keys_per_block;

                append_block_to_partition(pivots, memory, pivot_buffer_offset, keys_to_write,
                                          partition_write_seeks, partition_write_transfers);
            }
}

void process_partition(struct DiskFile& partition,int total_cnt,vector<int>& memory,struct DiskFile & Database,
                        int& curr_blk, int& curr_idx,int& read_seeks,int& read_transfers,int& write_seeks,int& write_transfers){
    
    int read_left = total_cnt;
    int blk = 0;
        
    //int usable_mem_size = mem_size;
    int usable_mem_size = mem_size - 1;
    int out_buffer_offset = usable_mem_size * keys_per_block;
    
    while(blk < (int)partition.blocks.size()){
        
        int blocks_to_read = min(usable_mem_size , (int)partition.blocks.size() - blk);
        int keys_loaded = 0;
        // read into memory
        for(int i = 0; i < blocks_to_read ; i++){
            int keys_in_this_block = min((int)partition.blocks[blk].size(), read_left);
            
            read_block_from_disk(partition, blk, memory, keys_loaded, keys_in_this_block, 
                                 read_seeks, read_transfers);
            
            keys_loaded += keys_in_this_block;
            read_left -= keys_in_this_block;
            blk++;
        }


        // write to database
        for(int i = 0; i < keys_loaded; i++){
//            write_transfers++;
                memory[out_buffer_offset + curr_idx] = memory[i];

                if(++curr_idx == keys_per_block){
                    overwrite_block_in_database(Database, curr_blk, memory, out_buffer_offset, keys_per_block, 
                                            write_seeks, write_transfers); 
                    curr_idx = 0;
                    curr_blk++;
                }
        }

    }
}

void merge_runs(struct DiskFile& Database , vector<int>& memory , struct DiskFile& left_partition ,int l_cnt ,
                                struct DiskFile& right_partition , int r_cnt, struct DiskFile& pivots , int p_cnt , 
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
                int out_buffer_offset = (mem_size - 1) * keys_per_block;

                overwrite_block_in_database(Database, curr_blk, memory, out_buffer_offset, curr_idx,
                                    merge_write_seeks, merge_write_transfers);
        }
}
void base_case(struct DiskFile& Database , int n , vector<int>& memory , int& partition_read_seeks , int& partition_read_transfers , int& merge_write_seeks , int& merge_write_transfers){
                int blocks = Database.blocks.size();
                int keys_loaded = 0;
                int keys_written = 0;
                int blk_id = 0;
                
                int is_empty = 0;
                // Empty Partition
                if(n == 0){
                        is_empty = 1;
                        goto out;
                }
                
                for(int i = 0 ; i < blocks ; i++){
                        int keys_in_this_block = min((int)Database.blocks[i].size(), n - keys_loaded);
                        if (keys_in_this_block <= 0) break;
                            
                        read_block_from_disk(Database, i, memory, keys_loaded, keys_in_this_block, 
                                                 partition_read_seeks, partition_read_transfers);
                            
                        keys_loaded += keys_in_this_block;
                }                                     
                
                sort(memory.begin() , memory.begin() + n);
                        
                for(int i = 0 ; i < blocks ; i++){
                        int keys_to_write = min((int)Database.blocks[i].size(), n - keys_written);
                        if (keys_to_write <= 0) break;
                    
                        overwrite_block_in_database(Database, i, memory, keys_written, keys_to_write, 
                                                        merge_write_seeks, merge_write_transfers);
                            
                        keys_written += keys_to_write;
                }        
out: 
        cost_pos[call_id] = (*cout_cost).tellp();

        (*cout_cost) << "\n====================================\n";
        (*cout_cost) << "        COST FOR CALL #" << call_id << "\n";
        (*cout_cost) << "====================================\n";
        if(is_empty) (*cout_cost) << "This is a call with an empty file \n";
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
        
        (*sout) << "Blocks: " << (int)Database.blocks.size() << "\n\n";
        if(is_empty) (*sout) << "This is a call with an empty file \n";

        for(auto &blk : Database.blocks){
            (*sout) << "Block " << setw(3) << blk_id++ << " : ";
            for(int x : blk){
                (*sout) << setw(6) << x;
            }
            (*sout) << "\n";
        }
}
void external_quicksort(struct DiskFile& Database , int n ){
 
         int seeks = 0;
         int transfers = 0;
         
         int partition_read_seeks = 0;
         int partition_write_seeks = 0;
         int merge_read_seeks = 0;
         int merge_write_seeks = 0;


         int partition_read_transfers = 0;
         int partition_write_transfers = 0;
         int merge_read_transfers = 0;
         int merge_write_transfers = 0;

         int this_partition = tot_partitions + 1;
         int this_call = call_id + 1;
         
         int l_cnt = 0;
         int r_cnt = 0;
         int p_cnt = 0;
                
         int blk_id = 0;
         
         vector<int> memory(mem_size * keys_per_block , INT32_MAX);
         struct DiskFile* left_partition = new DiskFile();
         struct DiskFile* right_partition = new DiskFile();
         struct DiskFile* pivots = new DiskFile();
        
         call_id++;
         if((int)Database.blocks.size() <= mem_size){
                 //base case 
                 base_case(Database , n , memory , partition_read_seeks , partition_read_transfers , merge_read_seeks , merge_write_transfers);
                 goto out;
         }
         
         
         partition_runs(Database , n , memory , *left_partition ,l_cnt , 
                         *right_partition , r_cnt , *pivots , p_cnt ,partition_write_seeks ,
                         partition_write_transfers , partition_read_seeks , partition_read_transfers);
        (*pout) << "\n====================================\n";
        (*pout) << "        PARTITION PASS #" << this_partition << "\n";
        (*pout) << "====================================\n";

        (*pout) << "\n[LEFT RUNS]\n";
        for(auto &blk : left_partition->blocks){
                (*pout) << "  ";
                for(int x : blk) (*pout) << setw(6) << x;
                        (*pout) << "\n";
        }

        (*pout) << "\n[PIVOT RUNS]\n";
        for(auto &blk : pivots->blocks){
                (*pout) << "  ";
                for(int x : blk) (*pout) << setw(6) << x;
                        (*pout) << "\n";
        }

        (*pout) << "\n[RIGHT RUNS]\n";
        for(auto &blk : right_partition->blocks){
            (*pout) << "  ";
            for(int x : blk) (*pout) << setw(6) << x;
            (*pout) << "\n";
        }
        
         tot_partitions++;

         external_quicksort(*left_partition ,l_cnt);
         external_quicksort(*right_partition ,r_cnt);

         merge_runs(Database , memory , *left_partition , l_cnt , 
                         *right_partition , r_cnt , *pivots , p_cnt , 
                         merge_read_seeks , merge_read_transfers , merge_write_seeks , merge_write_transfers);
                

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

        (*sout) << "Blocks: " << Database.blocks.size() << "\n\n";

        for(auto &blk : Database.blocks){
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
        
        delete left_partition;
        delete right_partition;
        delete pivots;
}

int main(int argc , char * argv[]){
        if(argc != 6){
                usage();
        }
        
        string input_file = argv[1];
        num_keys = stoi(argv[2]);
        key_size = stoi(argv[3]);
        block_size = stoi(argv[4]);
        mem_size = stoi(argv[5]);
        

        // assuming that keys are not broken across blocks , as in if block size = 10 and key size  = 4 , then in 2 blocks there are 4 keys not 5.
        keys_per_block = block_size / key_size ;
        int num_blocks = num_keys / keys_per_block + ((num_keys % keys_per_block) != 0);
        
        struct DiskFile Database;
        Database.blocks.assign(num_blocks , vector<int>(keys_per_block , INT32_MAX));

        int curr_block = 0;
        int curr_count = 0;
        
        ofstream partition_file , cost_file , sort_file;
        ifstream printing_file;

        ifstream fin(input_file);
        if(!fin){
                cout << "Error opening file";
                usage();
        }
        
        if(num_keys <= 0 || key_size <= 0 || block_size <= 0 || mem_size <= 0){
                cout << "Error : All inputs should be positive\n";
                usage();
        }
        
        if(block_size < key_size){
                cout << "Error : Block size should be greater than size of key\n";
                usage();      
        }

        if(mem_size - 3 <= 0) {
                cout << "Error : Memory must be atleast 3 blocks for partitioning\n";
                usage();
        }

        
        int key_count = 0;
        string line;
        
        while(getline(fin , line)){
                if (line.empty() || line.find_first_not_of(" \r\n\t") == string::npos) {
                        continue;
                }

                if(curr_block >= num_blocks) break;

                Database.blocks[curr_block][curr_count++] = stoi(line);
                key_count++;   

                if(curr_count == keys_per_block){
                        curr_count = 0;
                        curr_block++;
                }
        } 

        fin.close();
        
        if(key_count != num_keys) {
                cout << "No of keys provided did not match with number of keys in given file" << endl;
                cout << "No of keys in this file : " << key_count << endl;
                usage();
        } 

        partition_file.open("partition_log.txt");
        cost_file.open("cost_log.txt");
        sort_file.open("sorted_log.txt");
        
        int fpartition = 0;
        if(partition_file.is_open()){
                pout = &partition_file;
                fpartition = 1;
        }
        int fcost = 0;
        if(cost_file.is_open()){
                 cout_cost = &cost_file;
                 fcost = 1;
        }
        int fsort = 0;
        if(sort_file.is_open()){
                sout = &sort_file;
                fsort = 1;
        }        

        external_quicksort(Database , num_keys);
        
        partition_file.close();
        cost_file.close();
        sort_file.close();

        if(!fpartition) goto cost;   
        cout << "\n===== PARTITION LOG =====\n";
        printing_file.open("partition_log.txt");

        while(getline(printing_file, line)){
                cout << line << endl;
        }
        printing_file.close();
cost:
        if(!fcost) goto sort;
        cout << "\n===== COST LOG =====\n";
        printing_file.open("cost_log.txt");

        for(int i = 1; i <= call_id; i++){
                printing_file.clear();
                printing_file.seekg(cost_pos[i]);

                string line;
                while(getline(printing_file , line)){
                        if(line.find("COST FOR CALL #") != string::npos && line.find('#' + to_string(i)) == string::npos){
                                break;
                        }
                        cout << line << endl;
                }
        }
        printing_file.close();
sort:        
        if(!fsort) goto out;
        cout << "\n=========== SORTED OUTPUT ===========\n";

        printing_file.open("sorted_log.txt");

        for(int i = 1; i <= call_id; i++){
            printing_file.clear();
            printing_file.seekg(sorted_pos[i]);

            string line;
            while(getline(printing_file, line)){
                if(line.find("SORTED OUTPUT AFTER CALL #") != string::npos &&
                   line.find("#" + to_string(i)) == string::npos){
                        break;
                }
                cout << line << endl;
            }
        }
        printing_file.close();

        cout << "\nTotal partition passes: " << tot_partitions << endl;
        cout << "Total seeks: " << tot_seeks << endl;
        cout << "Total transfers: " << tot_transfers << endl;
out:
        return 0;
}
