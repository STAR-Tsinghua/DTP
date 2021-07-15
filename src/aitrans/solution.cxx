#include <iostream>
#include <memory>
#include <algorithm>
#include <vector>
#include <cmath>
#include <ctime>
#include <cstring>
// #include <torch/script.h> // One-stop header.

#include "solution.hxx"
// parameter.py
const static int MAX_P = 3;
const static int time_interval = 2000;
const static double MAX_BW = 200.0 * 1000 * 1000 * 8;
const static double MAX_DDL = 0.2;
const static int BYTES_PER_PACKET = 1500;

uint64_t SolutionAckRatio() {
    return 4;
}

bool SolutionShouldDropBlock(struct Block *block, double bandwidth, double rtt,uint64_t next_packet_id, uint64_t current_time) {
    return current_time - block->block_create_time > block->block_deadline;
}

float SolutionRedundancy(){

    if(your_parameter["current_block_remainning_time"] < float_parameter["rtt"] * 2){
        // cout  << "rudundancy func if1:" << float_parameter["redundancy"] << endl;
        return float_parameter["redundancy"];
    }
    else if (((your_parameter["current_block_remainning_size"] * 1.0 )/ your_parameter["current_block_remainning_time"]) * (1 + ((your_parameter["lost_pkt_num"] * 1.0) / your_parameter["total_pkt_num"])) * 8 < float_parameter["current_rate"]){
        // cout  << "rudundancy func if2:" << float_parameter["redundancy"] << endl;
        return float_parameter["redundancy"];
    }
    else{
        // cout  << "rudundancy func:" << 0 << endl;
        return 0.0;
    }
}

double get_number_res_from_order(char* order) {
    FILE* fp = NULL;
    double ret = 0;
    char buf[100];

    // call cmd in terminal
    fp = popen(order, "r");
    if (!fp) {
        printf("error in poen");
        return -1;
    }

    // get cmd output
    memset(buf, 0, sizeof(buf));
    fgets(buf, sizeof(buf)-1, fp);

    // parse double number
    ret = atof(buf);

    pclose(fp);
    return ret;
}

void SolutionInit(uint64_t *init_congestion_window, uint64_t *init_pacing_rate)
{

    for(int i = 0;i < 30;i ++){
        your_parameter[std::to_string(i)] = 0;
    }
    your_parameter["index"] = 0;

    your_parameter["total_pkt_num"] = 0;
    your_parameter["lost_pkt_num"] = 0;

    your_parameter["interval_pkt_num"] = 0;
    your_parameter["interval_start_time"] = 0;
    your_parameter["interval_send_num"] = 0;

    float_parameter["rtt"] = -10000;

    your_parameter["ddl"] = 0;
    your_parameter["size"] = 0;
    your_parameter["prio"] = 0;

    your_parameter["rtt_update_time"] = 0;
    your_parameter["last_block_id"] = -1;

    float_parameter["current_rate"] = MAX_BW;
    float_parameter["redundancy"] = 0.0;

    your_parameter["current_block_remainning_time"] = 2100000000;
    your_parameter["current_block_remainning_size"] = 0;

    *init_pacing_rate = MAX_BW;
    *init_congestion_window = 12345678;

}

uint64_t SolutionSelectBlock(Block* blocks, uint64_t block_num, uint64_t next_packet_id, uint64_t current_time)
{

    your_parameter["interval_send_num"] += 1;

    if (float_parameter["rtt"] > 0.0 && your_parameter["rtt_update_time"] + float_parameter["rtt"] < current_time){
        your_parameter["rtt_update_time"] = current_time;
        float_parameter["rtt"] = -10000.0;
    }
        
    double Min_weight = 10000000.0;
    int Min_weight_block_id = -1;

    int len = -1;
    int ddl, size, prio;
    for(int i = 0;i < block_num;i ++){
        if(blocks[i].block_id == your_parameter["last_block_id"]){
            len = blocks[i].remaining_size;
            ddl = blocks[i].block_deadline;
            size = blocks[i].remaining_size;
            prio = blocks[i].block_priority;
        }
    }

    if (len <= 0){

        for(int i = 0;i < block_num;i ++){
            if(blocks[i].remaining_size > 0){
                int tempddl = blocks[i].block_deadline;
                int Passedtime = current_time - blocks[i].block_create_time;
                double One_way_delay = float_parameter["rtt"];
                int tempsize = blocks[i].remaining_size;
                double Current_sending_rate = float_parameter["current_rate"];

                double Remaining_time = tempddl - Passedtime - One_way_delay - ((tempsize * 8.0) / Current_sending_rate) * 1000;

                if(Remaining_time >= 0.0){
                    int tempprio = blocks[i].block_priority;
                    double weight = (1.0 * Remaining_time / ddl) / (1 - 1.0 * tempprio / MAX_P);
                    if(Min_weight_block_id == -1){
                        Min_weight_block_id = i;
                        Min_weight = weight;
                        ddl = blocks[i].block_deadline;
                        size = blocks[i].remaining_size;
                        prio = blocks[i].block_priority;
                    }
                    else if(Min_weight > weight){
                        Min_weight_block_id = i;
                        Min_weight = weight;
                        ddl = blocks[i].block_deadline;
                        size = blocks[i].remaining_size;
                        prio = blocks[i].block_priority;
                    }
                    else if(Min_weight == weight && blocks[i].remaining_size < blocks[Min_weight_block_id].remaining_size){
                        Min_weight_block_id = i;
                        Min_weight = weight;
                        ddl = blocks[i].block_deadline;
                        size = blocks[i].remaining_size;
                        prio = blocks[i].block_priority;
                    }
                }
            }
        }

        your_parameter["ddl"] = ddl;
        your_parameter["size"] = size;
        your_parameter["prio"] = prio;

        your_parameter["last_block_id"] = Min_weight_block_id;

        if(Min_weight_block_id != -1){
            your_parameter["last_block_id"] = blocks[Min_weight_block_id].block_id;
            return your_parameter["last_block_id"];
        }   
        else {
            return blocks[0].block_id;
        }
    }
    else {
        your_parameter["ddl"] = ddl;
        your_parameter["size"] = size;
        your_parameter["prio"] = prio;

        return your_parameter["last_block_id"];
    }
}

void SolutionCcTrigger(CcInfo *cc_infos, uint64_t cc_num, uint64_t *congestion_window, uint64_t *pacing_rate)
{
    /************** START CODE HERE ***************/
    uint64_t cwnd = *congestion_window;
    vector<int> cc_types, rtt_sample;
    double loss_nums = 0, rtt_sum = 0;
    uint64_t current_time = 0;

    for (uint64_t i = 0; i < cc_num; i++)
    {
        char event_type = cc_infos[i].event_type;

        if(event_type == 'D'){
            your_parameter["lost_pkt_num"] += 1;
            your_parameter[std::to_string(your_parameter["index"])] = 1;
        }
        else{
            your_parameter[std::to_string(your_parameter["index"])] = 0;
            if(float_parameter["rtt"] < 0.0){
                float_parameter["rtt"] = cc_infos[i].rtt;
            }
            else {
                float_parameter["rtt"] = float_parameter["rtt"] * 0.8 + cc_infos[i].rtt * 0.2;
            }
            your_parameter["rtt_update_time"] = max(your_parameter["rtt_update_time"], cc_infos[i].event_time);
            your_parameter["interval_pkt_num"] += 1;
        }
        your_parameter["index"] += 1;
        if(your_parameter["index"] >= 30)
            your_parameter["index"] = 0;
        your_parameter["total_pkt_num"] += 1;

        current_time = max(current_time, cc_infos[i].event_time);
    }

    if(current_time > your_parameter["interval_start_time"] + time_interval){

        // cout  << "update module at " << current_time << endl;

        clock_t start, finish; 
        double duration; 

        start = clock();
        // torch::jit::Module module;
        // try
        // {
        //     module = torch::jit::load("model.pt", c10::kCPU);

        //     std::vector<torch::jit::IValue> inputs;

        //     torch::Tensor scales_ = torch::zeros({1,1,9});

        //     scales_[0][0][0] = 1.0 * your_parameter["ddl"] / 1000.0 / MAX_DDL / 100.0;
        //     scales_[0][0][1] = 1.0 * your_parameter["size"] / (MAX_BW / 8) / 10.0;
        //     scales_[0][0][2] = (1.0 * your_parameter["prio"] + 1.0) / MAX_P / 100.0;

        //     scales_[0][0][3] = 1.0 * your_parameter["interval_pkt_num"] * BYTES_PER_PACKET * 8 / (current_time - your_parameter["interval_start_time"]) / MAX_BW / 100.0;

        //     if(float_parameter["rtt"] >= 0.0)
        //         scales_[0][0][4] = float_parameter["rtt"] / 1000.0 / MAX_DDL / 10.0;

        //     if(your_parameter["total_pkt_num"] > 0)
        //         scales_[0][0][5] = 1.0 * your_parameter["lost_pkt_num"] / your_parameter["total_pkt_num"] / 10.0;

        //     scales_[0][0][6] = 1.0 * your_parameter["interval_send_num"] * BYTES_PER_PACKET * 8 / (current_time - your_parameter["interval_start_time"]) / MAX_BW / 100.0;
        //     scales_[0][0][7] = float_parameter["current_rate"] / MAX_BW / 100.0;
        //     scales_[0][0][8] = float_parameter["redundancy"] / MAX_BW / 100.0;

        //     // cout  << "model input:";
        //     // for(int i = 0;i < 9;i ++)cout << scales_[0][0][i].item().toDouble() << " ";
        //     // cout << endl;
            
        //     inputs.push_back(scales_);
        //     at::Tensor output = module.forward(inputs).toTensor();
            
        //     // cout << "model output:";
        //     // for(int i = 0;i < 2;i ++)cout << output[0][i].item().toDouble() << " ";
        //     // cout << endl;
            
        //     float_parameter["current_rate"] = (tanh(output[0][0].item().toDouble()) * 0.5 + 0.5) * MAX_BW * 2;
        //     float_parameter["redundancy"] = (tanh(output[0][1].item().toDouble()) * 0.5 + 0.5);

        //     // cout << float_parameter["current_rate"] << " pacing rate" << endl;
        //     // cout << float_parameter["redundancy"] << " redundancy" << endl;

        // }
        // catch (const c10::Error &e)
        // {
        //     std::cout << e.msg() <<  endl;
        //     std::cerr << "error loading the model\n";
        // }
        
        finish = clock();
        duration = (double)(finish - start) / CLOCKS_PER_SEC; 
        // cout << "update time cost:" << duration << endl;

        your_parameter["interval_pkt_num"] = 0;
        your_parameter["interval_start_time"] = current_time;
        your_parameter["interval_send_num"] = 0;
    }

    *pacing_rate = float_parameter["current_rate"];

    // std::cout << "pacing rate:" << *pacing_rate << std::endl;
}
