/**
 * CS537: Intro to OS, Final Project
 * Automated Temperature Regulation System for Chicken Storage
 * 
 * This system monitors and regulates temperature to ensure chicken remains
 * at a safe temperature (neither too cold nor too warm), with different 
 * requirements than other meats like beef.
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <pthread.h>
 #include <unistd.h>
 #include <stdbool.h>
 #include <string.h>
 #include <time.h>
 #include <signal.h>
 
 /* Global constants */
 #define CHICKEN_MIN_TEMP 35.0  // Minimum safe temperature (F)
 #define CHICKEN_MAX_TEMP 40.0  // Maximum safe temperature (F)
 #define COOLING_RATE 0.5       // How much temperature drops per cooling cycle
 #define HEATING_RATE 0.4       // How much temperature rises per heating cycle
 #define AMBIENT_VARIANCE 1.0   // Maximum random variance in ambient temperature
 #define CYCLE_TIME_SEC 1       // Time between cycles in seconds
 
 /* Global variables */
 typedef struct {
     double current_temp;       // Current temperature
     double target_temp;        // Target temperature
     bool cooler_on;            // Cooling system status
     bool heater_on;            // Heating system status
     bool system_running;       // Overall system status
     pthread_mutex_t lock;      // Mutex for thread safety
 } EnterpriseState;
 
 EnterpriseState enterprise;
 
 /* Database structures as described in the CSO pipeline */
 typedef struct {
     double temp;               // Temperature reading
     double ambient_temp;       // Ambient temperature (external)
     time_t timestamp;          // When reading was taken
     // Add other inputs as needed
 } InputSet;  // {i}
 
 typedef struct {
     bool temp_too_high;
     bool temp_too_low;
     bool temp_changing_rapidly;
     double temp_delta;
     time_t detection_time;
 } EventPattern;  // {e}
 
 typedef struct {
     bool situation_detected;
     char situation_desc[100];
     EventPattern events;
     time_t assessment_time;
 } SituationOfInterest;  // {s}
 
 typedef struct {
     bool activate_cooler;
     bool activate_heater;
     bool maintain_current;
     double adjustment_amount;
     char action_desc[100];
 } CourseOfAction;  // {c}
 
 typedef struct {
     CourseOfAction action;
     bool complies_with_policy;
     char compliance_notes[100];
 } PlanOfAction;  // {a}
 
 typedef struct {
     PlanOfAction plan;
     bool resources_assigned;
     char resource_list[100];
 } PlanOfRecord;  // {p}
 
 typedef struct {
     char task_name[50];
     void (*task_function)(void*);
     void* task_args;
     bool is_complete;
 } ExecutableTask;  // {t}
 
 typedef struct {
     double new_temp;
     bool cooler_status;
     bool heater_status;
     time_t execution_time;
 } OutputSet;  // {o}
 
 /* Thread function prototypes */
 void* enterprise_thread(void* arg);           // Simulates the physical process (chicken storage)
 void* situation_assessment_thread(void* arg); // SAS - monitors enterprise state
 void* plan_generation_thread(void* arg);      // PGS - determines response
 void* plan_execution_thread(void* arg);       // PES - executes plans
 
 /* Database function prototypes */
 void db_init();
 void db_save_input(InputSet input);
 void db_save_event(EventPattern event);
 void db_save_situation(SituationOfInterest situation);
 void db_save_action(CourseOfAction action);
 void db_save_plan(PlanOfAction plan);
 void db_save_record(PlanOfRecord record);
 void db_save_task(ExecutableTask task);
 void db_save_output(OutputSet output);
 
 /* Command line interface functions */
 void process_command(char* command);
 void display_help();
 void display_status();
 
 /* Global thread safety */
 pthread_mutex_t db_mutex = PTHREAD_MUTEX_INITIALIZER;
 pthread_cond_t sas_cond = PTHREAD_COND_INITIALIZER;
 pthread_cond_t pgs_cond = PTHREAD_COND_INITIALIZER;
 pthread_cond_t pes_cond = PTHREAD_COND_INITIALIZER;
 
 /* Thread communication variables */
 InputSet latest_input;
 SituationOfInterest latest_situation;
 PlanOfAction latest_plan;
 PlanOfRecord latest_record;
 OutputSet latest_output;
 
 /* Global control */
 volatile bool system_running = true;
 
 /**
  * Enterprise Thread - Simulates the chicken storage environment
  * with temperature fluctuations
  */
 void* enterprise_thread(void* arg) {
     printf("Enterprise thread started - simulating chicken storage environment\n");
     
     // Initialize with a starting temperature
     enterprise.current_temp = 38.0; // Starting temperature
     enterprise.target_temp = 37.5;  // Ideal temperature for chicken
     enterprise.cooler_on = false;
     enterprise.heater_on = false;
     
     while (system_running) {
         // Add random variation to simulate environmental factors
         double ambient_effect = ((double)rand() / RAND_MAX * 2 - 1) * AMBIENT_VARIANCE;
         
         pthread_mutex_lock(&enterprise.lock);
         
         // Calculate new temperature based on current systems
         if (enterprise.cooler_on) {
             enterprise.current_temp -= COOLING_RATE;
         }
         
         if (enterprise.heater_on) {
             enterprise.current_temp += HEATING_RATE;
         }
         
         // Apply ambient temperature effect
         enterprise.current_temp += ambient_effect;
         
         // Create input set for the current cycle
         InputSet input;
         input.temp = enterprise.current_temp;
         input.ambient_temp = enterprise.current_temp - ambient_effect; // Simplified
         input.timestamp = time(NULL);
         
         // Save to database
         pthread_mutex_lock(&db_mutex);
         db_save_input(input);
         latest_input = input;
         pthread_mutex_unlock(&db_mutex);
         
         // Signal SAS that new data is available
         pthread_cond_signal(&sas_cond);
         
         pthread_mutex_unlock(&enterprise.lock);
         
         // Sleep for cycle time
         sleep(CYCLE_TIME_SEC);
     }
     
     printf("Enterprise thread terminating\n");
     return NULL;
 }
 
 /**
  * Situation Assessment Service (SAS) Thread
  * Monitors the enterprise state looking for situations of interest
  */
 void* situation_assessment_thread(void* arg) {
     printf("SAS thread started - monitoring chicken storage temperature\n");
     
     while (system_running) {
         pthread_mutex_lock(&db_mutex);
         
         // Wait for new input
         pthread_cond_wait(&sas_cond, &db_mutex);
         
         // Assess the situation based on latest input
         SituationOfInterest situation;
         situation.situation_detected = false;
         situation.assessment_time = time(NULL);
         
         // Check if temperature is too high
         if (latest_input.temp > CHICKEN_MAX_TEMP) {
             situation.situation_detected = true;
             situation.events.temp_too_high = true;
             situation.events.temp_too_low = false;
             sprintf(situation.situation_desc, "Temperature too high: %.1f°F", latest_input.temp);
         }
         // Check if temperature is too low
         else if (latest_input.temp < CHICKEN_MIN_TEMP) {
             situation.situation_detected = true;
             situation.events.temp_too_high = false;
             situation.events.temp_too_low = true;
             sprintf(situation.situation_desc, "Temperature too low: %.1f°F", latest_input.temp);
         }
         // Normal temperature
         else {
             situation.events.temp_too_high = false;
             situation.events.temp_too_low = false;
             sprintf(situation.situation_desc, "Temperature normal: %.1f°F", latest_input.temp);
         }
         
         // Save situation to database
         db_save_situation(situation);
         latest_situation = situation;
         
         // Signal PGS if situation detected
         if (situation.situation_detected) {
             pthread_cond_signal(&pgs_cond);
         }
         
         pthread_mutex_unlock(&db_mutex);
     }
     
     printf("SAS thread terminating\n");
     return NULL;
 }
 
 /**
  * Plan Generation Service (PGS) Thread
  * Determines logical response to current situation
  */
 void* plan_generation_thread(void* arg) {
     printf("PGS thread started - generating temperature control plans\n");
     
     while (system_running) {
         pthread_mutex_lock(&db_mutex);
         
         // Wait for new situation
         pthread_cond_wait(&pgs_cond, &db_mutex);
         
         // Generate course of action based on situation
         CourseOfAction action;
         strcpy(action.action_desc, "No action needed");
         action.activate_cooler = false;
         action.activate_heater = false;
         action.maintain_current = true;
         action.adjustment_amount = 0.0;
         
         if (latest_situation.events.temp_too_high) {
             strcpy(action.action_desc, "Activate cooling");
             action.activate_cooler = true;
             action.activate_heater = false;
             action.maintain_current = false;
             action.adjustment_amount = CHICKEN_MAX_TEMP - latest_input.temp;
         }
         else if (latest_situation.events.temp_too_low) {
             strcpy(action.action_desc, "Activate heating");
             action.activate_cooler = false;
             action.activate_heater = true;
             action.maintain_current = false;
             action.adjustment_amount = CHICKEN_MIN_TEMP - latest_input.temp;
         }
         
         // Create plan of action
         PlanOfAction plan;
         plan.action = action;
         plan.complies_with_policy = true; // Simplified compliance check
         strcpy(plan.compliance_notes, "Standard temperature regulation policy");
         
         // Save plan to database
         db_save_action(action);
         db_save_plan(plan);
         latest_plan = plan;
         
         // Create plan of record
         PlanOfRecord record;
         record.plan = plan;
         record.resources_assigned = true;
         strcpy(record.resource_list, "Cooler, Heater, Temperature sensor");
         
         // Save record to database
         db_save_record(record);
         latest_record = record;
         
         // Signal PES
         pthread_cond_signal(&pes_cond);
         
         pthread_mutex_unlock(&db_mutex);
     }
     
     printf("PGS thread terminating\n");
     return NULL;
 }
 
 /**
  * Plan Execution Service (PES) Thread
  * Executes plans to achieve desired state
  */
 void* plan_execution_thread(void* arg) {
     printf("PES thread started - executing temperature control plans\n");
     
     while (system_running) {
         pthread_mutex_lock(&db_mutex);
         
         // Wait for new plan
         pthread_cond_wait(&pes_cond, &db_mutex);
         
         // Execute the plan
         pthread_mutex_lock(&enterprise.lock);
         
         if (latest_record.plan.action.activate_cooler) {
             enterprise.cooler_on = true;
             enterprise.heater_on = false;
             printf("ACTION: Turning on cooler, temperature: %.1f°F\n", enterprise.current_temp);
         }
         else if (latest_record.plan.action.activate_heater) {
             enterprise.cooler_on = false;
             enterprise.heater_on = true;
             printf("ACTION: Turning on heater, temperature: %.1f°F\n", enterprise.current_temp);
         }
         else {
             enterprise.cooler_on = false;
             enterprise.heater_on = false;
             printf("ACTION: Maintaining current systems, temperature: %.1f°F\n", enterprise.current_temp);
         }
         
         // Create output set
         OutputSet output;
         output.new_temp = enterprise.current_temp;
         output.cooler_status = enterprise.cooler_on;
         output.heater_status = enterprise.heater_on;
         output.execution_time = time(NULL);
         
         // Save output to database
         db_save_output(output);
         latest_output = output;
         
         pthread_mutex_unlock(&enterprise.lock);
         pthread_mutex_unlock(&db_mutex);
     }
     
     printf("PES thread terminating\n");
     return NULL;
 }
 
 /* Database function implementations - simplified for skeleton */
 void db_init() {
     // Initialize database structures
     printf("Initializing CSO database structures...\n");
 }
 
 void db_save_input(InputSet input) {
     // Save input to database
 }
 
 void db_save_event(EventPattern event) {
     // Save event to database
 }
 
 void db_save_situation(SituationOfInterest situation) {
     // Save situation to database
 }
 
 void db_save_action(CourseOfAction action) {
     // Save action to database
 }
 
 void db_save_plan(PlanOfAction plan) {
     // Save plan to database
 }
 
 void db_save_record(PlanOfRecord record) {
     // Save record to database
 }
 
 void db_save_task(ExecutableTask task) {
     // Save task to database
 }
 
 void db_save_output(OutputSet output) {
     // Save output to database
 }
 
 /* Command processing functions */
 void process_command(char* command) {
     if (strcmp(command, "help") == 0 || strcmp(command, "h") == 0) {
         display_help();
     }
     else if (strcmp(command, "status") == 0 || strcmp(command, "s") == 0) {
         display_status();
     }
     else if (strcmp(command, "quit") == 0 || strcmp(command, "q") == 0) {
         system_running = false;
         printf("Shutting down system...\n");
     }
     else if (strcmp(command, "cooler-on") == 0) {
         pthread_mutex_lock(&enterprise.lock);
         enterprise.cooler_on = true;
         printf("Manual override: Cooler turned ON\n");
         pthread_mutex_unlock(&enterprise.lock);
     }
     else if (strcmp(command, "cooler-off") == 0) {
         pthread_mutex_lock(&enterprise.lock);
         enterprise.cooler_on = false;
         printf("Manual override: Cooler turned OFF\n");
         pthread_mutex_unlock(&enterprise.lock);
     }
     else if (strcmp(command, "heater-on") == 0) {
         pthread_mutex_lock(&enterprise.lock);
         enterprise.heater_on = true;
         printf("Manual override: Heater turned ON\n");
         pthread_mutex_unlock(&enterprise.lock);
     }
     else if (strcmp(command, "heater-off") == 0) {
         pthread_mutex_lock(&enterprise.lock);
         enterprise.heater_on = false;
         printf("Manual override: Heater turned OFF\n");
         pthread_mutex_unlock(&enterprise.lock);
     }
     else {
         printf("Unknown command: %s\n", command);
         display_help();
     }
 }
 
 void display_help() {
     printf("\n--- Chicken Temperature Control System ---\n");
     printf("Available commands:\n");
     printf("  status (s)    : Display current system status\n");
     printf("  help (h)      : Display this help message\n");
     printf("  cooler-on     : Manually turn on the cooling system\n");
     printf("  cooler-off    : Manually turn off the cooling system\n");
     printf("  heater-on     : Manually turn on the heating system\n");
     printf("  heater-off    : Manually turn off the heating system\n");
     printf("  quit (q)      : Exit the program\n");
 }
 
 void display_status() {
     pthread_mutex_lock(&enterprise.lock);
     printf("\n--- System Status ---\n");
     printf("Current temperature: %.1f°F\n", enterprise.current_temp);
     printf("Target temperature: %.1f°F\n", enterprise.target_temp);
     printf("Safe range: %.1f°F - %.1f°F\n", CHICKEN_MIN_TEMP, CHICKEN_MAX_TEMP);
     printf("Cooling system: %s\n", enterprise.cooler_on ? "ON" : "OFF");
     printf("Heating system: %s\n", enterprise.heater_on ? "ON" : "OFF");
     pthread_mutex_unlock(&enterprise.lock);
 }
 
 /**
  * Main function
  */
 int main() {
     pthread_t enterprise_tid, sas_tid, pgs_tid, pes_tid;
     char command[50];
     
     // Initialize random seed
     srand(time(NULL));
     
     // Initialize enterprise state mutex
     pthread_mutex_init(&enterprise.lock, NULL);
     
     // Initialize database
     db_init();
     
     printf("Starting Automated Temperature Regulation System for Chicken Storage\n");
     printf("Optimal temperature range: %.1f°F - %.1f°F\n", CHICKEN_MIN_TEMP, CHICKEN_MAX_TEMP);
     
     // Create threads
     pthread_create(&enterprise_tid, NULL, enterprise_thread, NULL);
     pthread_create(&sas_tid, NULL, situation_assessment_thread, NULL);
     pthread_create(&pgs_tid, NULL, plan_generation_thread, NULL);
     pthread_create(&pes_tid, NULL, plan_execution_thread, NULL);
     
     // Command line interface
     display_help();
     
     while (system_running) {
         printf("\n> ");
         fgets(command, 50, stdin);
         
         // Remove newline character
         command[strcspn(command, "\n")] = 0;
         
         process_command(command);
     }
     
     // Wait for threads to terminate
     pthread_join(enterprise_tid, NULL);
     pthread_join(sas_tid, NULL);
     pthread_join(pgs_tid, NULL);
     pthread_join(pes_tid, NULL);
     
     // Clean up
     pthread_mutex_destroy(&enterprise.lock);
     pthread_mutex_destroy(&db_mutex);
     pthread_cond_destroy(&sas_cond);
     pthread_cond_destroy(&pgs_cond);
     pthread_cond_destroy(&pes_cond);
     
     printf("System shutdown complete\n");
     
     return 0;
 }