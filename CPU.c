/**************************************************************/
/* CS/COE 1541
   just compile with gcc -o pipeline pipeline.c
   and execute using
   ./pipeline  /afs/cs.pitt.edu/courses/1541/short_traces/sample.tr	0
***************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include "CPU.h"

int main(int argc, char **argv)
{
   // define constants
   const int pipeline_size = 5;

   // variable declarations
   struct trace_item *tr_entry;
   struct trace_item *no_op;
   size_t size;
   char *trace_file_name;
   int trace_view_on = 0;
   int branch_prediction_on = 0;
   int lw_hazard_detected = 0;
   int hazard_buffer_skip = 0;
   unsigned int cycle_number = 0;

   struct pipeline *my_pipeline; //creates and initializes pipeline
   struct trace_item *pipeline[pipeline_size]; //NOT ITERATING PROPERLY!!!!!
   struct bpt_entry *bp_table[64]; //creates branch prediction table

   // Initialize pipeline with each step at NULL
   for(int i= 0;i<(sizeof(pipeline)/sizeof(struct trace_item *));i++)
      pipeline[i]= NULL;

   // Check for improper usage
   if (argc == 1 || argc > 4) {
      fprintf(stdout, "\nUSAGE: ./CPU <trace file> <trace switch> <branch prediction switch>");
      fprintf(stdout, "\n(trace switch) to turn on or off individual item view.");
      fprintf(stdout, "\n(brach prediction switch) to turn on or off branch prediction table.\n");
      exit(0);
   }

   // Handle input values and set necessary values
   trace_file_name = argv[1];
   if (argc >= 3) trace_view_on = atoi(argv[2]) ;
   if (argc == 4) branch_prediction_on = atoi(argv[3]);

   fprintf(stdout, "\n ** opening file %s\n", trace_file_name);

   // Open the trace file
   trace_fd = fopen(trace_file_name, "rb");

   // Handle trace file error
   if (!trace_fd) {
      fprintf(stdout, "\ntrace file %s not opened.\n\n", trace_file_name);
      exit(0);
   }

   // Initialize the trace buffer
   trace_init();

   // Begin simulation
   while(1) {
      if (!hazard_buffer_skip){
         // Check that there are remaining instructions in the buffer
         size = trace_get_item(&tr_entry);
      }
      // reset the buffer skip signal after 1 pass
      hazard_buffer_skip = 0;

      if(!size){
         tr_entry= NULL;

         if (pipeline[4]==NULL) {
            // No more instructions (trace_items) to simulate
            printf("+ Simulation terminates at cycle : %u\n", cycle_number);
            break;
         }
      }

      // Hazard Detection
      if (pipeline[1] != NULL){
         if (pipeline[1]->type == ti_LOAD) {
            // Load Word Detected
            if ((pipeline[1]->type == ti_RTYPE) || (pipeline[1]->type == ti_STORE) || (pipeline[1]->type == ti_BRANCH)){
               if ((pipeline[1]->dReg == pipeline[0]->sReg_a) || (pipeline[1]->dReg == pipeline[0]->sReg_b)){
                  // Load-Use Hazard Detected
                  lw_hazard_detected = 1;
               }
            } else if ((pipeline[1]->type == ti_ITYPE) || (pipeline[1]->type == ti_LOAD) || (pipeline[1]->type == ti_JRTYPE)){
               if(pipeline[1]->dReg == pipeline[0]->sReg_a){
                  // Load-Use Hazard Detected
                  lw_hazard_detected = 1;
               }
            }
         }
      }

      //Advance instructions through pipeline
      //   pipeline[0] => IF/ID
      //   pipeline[1] => ID/EX
      //   pipeline[2] => EX/MEM
      //   pipeline[3] => MEM/WB
      //   pipeline[4] => Output

      // Handle LOAD-USE Hazard
      if (lw_hazard_detected){
         // Advance the pipeline from the ID/EX Buffer on
         pipeline[4] = pipeline[3];
         pipeline[3] = pipeline[2];
         pipeline[2] = pipeline[1];

         // Create a bubble to insert to ID/EX
         no_op = (struct trace_item *)malloc(sizeof(struct trace_item));
         no_op->type   = 0;
         no_op->sReg_a = 255;
         no_op->sReg_b = 255;
         no_op->dReg   = 255;
         no_op->PC     = 0;
         no_op->Addr   = 0;

         // Insert the bubble
         pipeline[1] = no_op;

         // Unset the flag
         lw_hazard_detected = 0;
         hazard_buffer_skip = 1;
      } else {
         // Advance pipeline normally
         pipeline[4] = pipeline[3]; // MEM/WB to OUTPUT
         pipeline[3] = pipeline[2]; // EX/MEM to MEM/WB
         pipeline[2] = pipeline[1]; // ID/EX to EX/MEM
         pipeline[1] = pipeline[0]; // IF/ID to ID/EX
         pipeline[0] = tr_entry;    // PC to IF/ID
      }

      cycle_number++;
      if (trace_view_on) {/* print the executed instruction if trace_view_on=1 */
         if(cycle_number<pipeline_size) {
            printf("[cycle %d] No output, pipeline filling.\n", cycle_number);
         }
         else if(pipeline[4]!=NULL) {
            switch(pipeline[4]->type) {
               case ti_NOP:
                  free(pipeline[4]);
                  printf("[cycle %d] NOP:",cycle_number) ;
                  break;
               case ti_RTYPE:
                  printf("[cycle %d] RTYPE:",cycle_number) ;
                  printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(dReg: %d)\n", pipeline[4]->PC, pipeline[4]->sReg_a, pipeline[4]->sReg_b, pipeline[4]->dReg);
                  break;
               case ti_ITYPE:
                  printf("[cycle %d] ITYPE:",cycle_number) ;
                  printf(" (PC: %x)(sReg_a: %d)(dReg: %d)(addr: %x)\n", pipeline[4]->PC, pipeline[4]->sReg_a, pipeline[4]->dReg, pipeline[4]->Addr);
                  break;
               case ti_LOAD:
                  printf("[cycle %d] LOAD:",cycle_number) ;
                  printf(" (PC: %x)(sReg_a: %d)(dReg: %d)(addr: %x)\n", pipeline[4]->PC, pipeline[4]->sReg_a, pipeline[4]->dReg, pipeline[4]->Addr);
                  break;
               case ti_STORE:
                  printf("[cycle %d] STORE:",cycle_number) ;
                  printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(addr: %x)\n", pipeline[4]->PC, pipeline[4]->sReg_a, pipeline[4]->sReg_b, pipeline[4]->Addr);
                  break;
               case ti_BRANCH:
                  printf("[cycle %d] BRANCH:",cycle_number) ;
                  printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(addr: %x)\n", pipeline[4]->PC, pipeline[4]->sReg_a, pipeline[4]->sReg_b, pipeline[4]->Addr);
                  break;
               case ti_JTYPE:
                  printf("[cycle %d] JTYPE:",cycle_number) ;
                  printf(" (PC: %x)(addr: %x)\n", pipeline[4]->PC,pipeline[4]->Addr);
                  break;
               case ti_SPECIAL:
                  printf("[cycle %d] SPECIAL:",cycle_number) ;
                  break;
               case ti_JRTYPE:
                  printf("[cycle %d] JRTYPE:",cycle_number) ;
                  printf(" (PC: %x) (sReg_a: %d)(addr: %x)\n", pipeline[4]->PC, pipeline[4]->dReg, pipeline[4]->Addr);
                  break;
            }
         }
      }
   }

   // Uninitialize the trace buffer
   trace_uninit();

   exit(0);
}
