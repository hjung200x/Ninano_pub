#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <fcntl.h>


#include "MQTTClient.h"
#include "json-c/json.h"

#define NINANO_PATH			"/var/ninano"
#define AM2315_PATH			"/var/ninano/AM2315"
#define NINANO_CONFIG		"/var/ninano/config.cfg"
//#define NINANO_ALARM_LOCK	"/var/ninano/lock"

/* Macros for logging */
#define LOG_DBG(x,...)              do { \
                                        time_t t; \
                                        struct tm *ti; \
                                        time(&t); \
                                        ti = localtime(&t); \
                                        printf("[%d/%02d/%02d %02d:%02d:%02d][DBG:%d] " x, \
                                            ti->tm_year+1900, ti->tm_mon+1, ti->tm_mday, \
                                            ti->tm_hour, ti->tm_min, ti->tm_sec, \
                                            __LINE__, ##__VA_ARGS__); \
                                        fflush(stdout); \
                                    } while(0);
#define LOG_INFO(x,...)              do { \
                                        time_t t; \
                                        struct tm *ti; \
                                        time(&t); \
                                        ti = localtime(&t); \
                                        printf("[%d/%02d/%02d %02d:%02d:%02d][INFO:%d] " x, \
                                            ti->tm_year+1900, ti->tm_mon+1, ti->tm_mday, \
                                            ti->tm_hour, ti->tm_min, ti->tm_sec, \
                                            __LINE__, ##__VA_ARGS__); \
                                        fflush(stdout); \
                                    } while(0);
#define LOG_WARN(x,...)              do { \
                                        time_t t; \
                                        struct tm *ti; \
                                        time(&t); \
                                        ti = localtime(&t); \
                                        printf("[%d/%02d/%02d %02d:%02d:%02d][WARN:%d] " x, \
                                            ti->tm_year+1900, ti->tm_mon+1, ti->tm_mday, \
                                            ti->tm_hour, ti->tm_min, ti->tm_sec, \
                                            __LINE__, ##__VA_ARGS__); \
                                        fflush(stdout); \
                                    } while(0);
#define LOG_ERR(x,...)              do { \
                                        time_t t; \
                                        struct tm *ti; \
                                        time(&t); \
                                        ti = localtime(&t); \
                                        printf("[%d/%02d/%02d %02d:%02d:%02d][ERR:%d] " x, \
                                            ti->tm_year+1900, ti->tm_mon+1, ti->tm_mday, \
                                            ti->tm_hour, ti->tm_min, ti->tm_sec, \
                                            __LINE__, ##__VA_ARGS__); \
                                        fflush(stdout); \
                                    } while(0);
#define LOG_NO_HEADER(x,...)        do { printf(x, ##__VA_ARGS__); fflush(stdout); } while(0);

typedef enum {
	CFG_TYPE_STRING,
	CFG_TYPE_INT,
	CFG_TYPE_DOUBLE,
	CFG_TYPE_END
} e_cfg_type;

typedef struct 
{
	char item_name[32];
	e_cfg_type type;
	union {
		void *str_value;
		int int_value;
		double double_value;
	} u;
} config_t;

config_t g_cfg[] = 
{
	{ "mqtt_host_addr", CFG_TYPE_STRING },
	{ "mqtt_client_id", CFG_TYPE_STRING },
	{ "mqtt_topic_temp_hum", CFG_TYPE_STRING },
	{ "mqtt_topic_control", CFG_TYPE_STRING },
	{ "mqtt_username", CFG_TYPE_STRING },
	{ "mqtt_password", CFG_TYPE_STRING },
	{ "mqtt_qos", CFG_TYPE_INT },
	{ "enable_alarm", CFG_TYPE_INT },
	{ "alarm_threshold_min_temp", CFG_TYPE_DOUBLE },
	{ "alarm_threshold_max_temp", CFG_TYPE_DOUBLE },
	{ "alarm_threshold_min_hum", CFG_TYPE_INT },
	{ "alarm_threshold_max_hum", CFG_TYPE_INT },
	{ "alarm_sms_appid", CFG_TYPE_STRING },
	{ "alarm_sms_apikey", CFG_TYPE_STRING },
	{ "alarm_sms_sender", CFG_TYPE_STRING },
	{ "alarm_sms_receiver", CFG_TYPE_STRING },
	{ "alarm_sms_interval", CFG_TYPE_INT },
	{ "", CFG_TYPE_END }
};

volatile MQTTClient_deliveryToken deliveredtoken;
MQTTClient client;
MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;

timer_t g_timer;
int g_alarm_lock = 0;


void check_threshold(double temp, double hum);
void time_handler_test_check_threshold(int sig, siginfo_t *info, void *p);


int create_timer( timer_t *timer_id, int sec, int msec, void (*func)(int, siginfo_t *, void *) )  
{  
    struct sigevent         te;  
    struct itimerspec       its;  
    struct sigaction        sa;  
    int                     sigNo = SIGRTMIN;  
   
    LOG_DBG("Enter...\n");

    /* Set up signal handler. */  
    sa.sa_flags = SA_SIGINFO;  
    sa.sa_sigaction = func;
    sigemptyset(&sa.sa_mask);  
  
    if (sigaction(sigNo, &sa, NULL) == -1)  
    {  
        LOG_ERR("sigaction error\n");
        return -1;  
    }  
    //LOG_DBG("SA_SIGINFO=%d\n", SA_SIGINFO);
    //LOG_DBG("SIGEV_SIGNAL=%d\n", SIGEV_SIGNAL);
   
    /* Set and enable alarm */  
    te.sigev_notify = SIGEV_SIGNAL;  
    te.sigev_signo = sigNo;  
    te.sigev_value.sival_ptr = timer_id;  
    timer_create(CLOCK_REALTIME, &te, timer_id);  
   
    its.it_interval.tv_sec = sec;
    its.it_interval.tv_nsec = msec * 1000000;  
    its.it_value.tv_sec = sec;
    
    its.it_value.tv_nsec = msec * 1000000;
    timer_settime(*timer_id, 0, &its, NULL);  
   
    LOG_DBG("Leave...\n");
    return 0;  
}

int get_config_value(char *item_name, config_t *cfg)
{
	int cnt = 0;

	while (g_cfg[cnt].type != CFG_TYPE_END)
	{
		if (!strcmp(item_name, g_cfg[cnt].item_name))
		{
			if (g_cfg[cnt].type == CFG_TYPE_STRING)
			{
				cfg->u.str_value = g_cfg[cnt].u.str_value;
			}
			else if (g_cfg[cnt].type == CFG_TYPE_INT)
			{
				cfg->u.int_value = g_cfg[cnt].u.int_value;
			}
			else if (g_cfg[cnt].type == CFG_TYPE_DOUBLE)
			{
				cfg->u.double_value = g_cfg[cnt].u.double_value;
			}

			return 0;
		}

		cnt++;
	}

	return -1;
}

int set_config_value(char *item_name, void *value)
{
	int cnt = 0;

	while (g_cfg[cnt].type != CFG_TYPE_END)
	{
		if (!strcmp(item_name, g_cfg[cnt].item_name))
		{
			if (g_cfg[cnt].type == CFG_TYPE_STRING)
			{
                char *str_value = (char *)value;
                int len;

                if (g_cfg[cnt].u.str_value)
                    free(g_cfg[cnt].u.str_value);

				len = strlen(str_value) + 1;
				g_cfg[cnt].u.str_value = calloc(1, len);
				strcpy(g_cfg[cnt].u.str_value, str_value);
			}
			else if (g_cfg[cnt].type == CFG_TYPE_INT)
			{
                int *int_value = (int *)value;

				g_cfg[cnt].u.int_value = *int_value;
			}
			else if (g_cfg[cnt].type == CFG_TYPE_DOUBLE)
			{
                double *double_value = (double *)value;

				g_cfg[cnt].u.double_value = *double_value;
			}

			return 0;
		}

		cnt++;
	}

	return -1;
}

int get_config_type(char *item_name)
{
	int cnt = 0;

    LOG_DBG("Enter...\n");

	while (g_cfg[cnt].type != CFG_TYPE_END)
	{
		if (!strcmp(item_name, g_cfg[cnt].item_name))
		{
            LOG_DBG("Leave...\n");
            return g_cfg[cnt].type;
        }

        cnt++;
    }

    LOG_DBG("Leave...\n");
    return -1;
}

void time_handler_remove_lock(int sig, siginfo_t *info, void *p)
{
    LOG_DBG("Enter...\n");
	//unlink(NINANO_ALARM_LOCK);
    LOG_INFO("Remove Alarm Lock, signo=%d, sigcode=%d\n", info->si_signo, info->si_code);
    g_alarm_lock = 0;

	timer_delete(g_timer);
    LOG_DBG("Leave...\n");
}

void delivered(void *context, MQTTClient_deliveryToken dt)
{
    LOG_DBG("Enter...\n");
    LOG_INFO("Message with token value %d delivery confirmed\n", dt);
    deliveredtoken = dt;
    LOG_DBG("Leave...\n");
}

void check_threshold(double temp, double hum)
{
	config_t cfg_enable_alarm, cfg_threshold_min_temp, cfg_threshold_max_temp;
	config_t cfg_threshold_min_hum, cfg_threshold_max_hum;
	config_t cfg_sms_appid, cfg_sms_apikey, cfg_sms_sender, cfg_sms_receiver, cfg_sms_interval;
	//struct stat sb;
	//int fd;
	char cmd[128];

    LOG_DBG("Enter...\n");

	get_config_value("enable_alarm", &cfg_enable_alarm);
	get_config_value("alarm_threshold_min_temp", &cfg_threshold_min_temp);
	get_config_value("alarm_threshold_max_temp", &cfg_threshold_max_temp);
	get_config_value("alarm_threshold_min_hum", &cfg_threshold_min_hum);
	get_config_value("alarm_threshold_max_hum", &cfg_threshold_max_hum);
	get_config_value("alarm_sms_appid", &cfg_sms_appid);
	get_config_value("alarm_sms_apikey", &cfg_sms_apikey);
	get_config_value("alarm_sms_sender", &cfg_sms_sender);
	get_config_value("alarm_sms_receiver", &cfg_sms_receiver);
	get_config_value("alarm_sms_interval", &cfg_sms_interval);

	//if (cfg_enable_alarm.u.int_value && stat(NINANO_ALARM_LOCK, &sb))
	if (cfg_enable_alarm.u.int_value && g_alarm_lock == 0)
	{
		if (temp < cfg_threshold_min_temp.u.double_value || temp > cfg_threshold_max_temp.u.double_value ||
			hum < cfg_threshold_min_hum.u.int_value || hum > cfg_threshold_max_hum.u.int_value)
		{
			sprintf(cmd, "%s/alarm.sh %s %s %s \"%s\" %.1f %.1f", NINANO_PATH, 
						(char *)cfg_sms_appid.u.str_value, (char *)cfg_sms_apikey.u.str_value, 
						(char *)cfg_sms_sender.u.str_value, (char *)cfg_sms_receiver.u.str_value,
                        temp, hum);
			system(cmd);
            LOG_INFO("!!!ALARM!!! Temperature: %.1f       Hummidity: %.1f\n", temp, hum);

            /*
			fd = creat(NINANO_ALARM_LOCK, 0644);
			if (fd > 0) close(fd);
            else perror("can't create lock file");
            */
            g_alarm_lock = 1;

            create_timer(&g_timer, cfg_sms_interval.u.int_value*60, 0, time_handler_remove_lock);
		}
	}
    else
    {
        if (cfg_enable_alarm.u.int_value == 0)
            LOG_DBG("Alarm disabled\n");

        //if (!stat(NINANO_ALARM_LOCK, &sb))
        if (g_alarm_lock)
            LOG_DBG("Alarm locked\n");
    }

    LOG_DBG("Leave...\n");
}

int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    int i;
    char* payloadptr;
    time_t rawtime;
    struct tm *t_info;
	config_t cfg_topic_temp_hum;
	config_t cfg_topic_control;

#if 0	// 이건 나중에 하자
    char *ptr_topic, *dup_topic, *token;

    ptr_topic = dup_topic = strdup(topicName);

    token = strsep(&dup_topic,"/");
    token = strsep(&dup_topic,"/");
    if (strcmp(token, "snu_pcb"))
    {
    	printf("unknown topic string [%s]\n", token);
    	goto _exit;
    }

    token = strsep(&dup_topic,"/");
    if (strcmp(token, "temp_hum"))
    {
    	printf("unknown topic string [%s]\n", token);
    	goto _exit;
    }

    token = strsep(&dup_topic,"/");
    printf("Device: %s\n", token);

    free(ptr_topic);
#endif

    LOG_DBG("Enter...\n");

	get_config_value("mqtt_topic_temp_hum", &cfg_topic_temp_hum);
	get_config_value("mqtt_topic_control", &cfg_topic_control);

    time(&rawtime);
    t_info = localtime(&rawtime);

    LOG_INFO("\nMessage arrived (%d/%02d/%02d %02d:%02d:%02d)\n",
        t_info->tm_year+1900, t_info->tm_mon+1, t_info->tm_mday, 
        t_info->tm_hour, t_info->tm_min, t_info->tm_sec);
    LOG_INFO("     topic: %s\n", topicName);
    LOG_NO_HEADER("   message: ");

    payloadptr = message->payload;
    for(i=0; i<message->payloadlen; i++)
    {
        LOG_NO_HEADER("%c", *payloadptr);
        payloadptr++;
    }
    LOG_NO_HEADER("\n");


    if (!strncmp(topicName, (char *)cfg_topic_temp_hum.u.str_value, strlen((char *)cfg_topic_temp_hum.u.str_value)))
    {
    	char device_name[16] = { '\0', };
    	char device_path[64] = { '\0', };
    	char datfile_path[64] = { '\0', };
    	FILE *fp;
    	json_object *root, *obj_device, *obj_temp, *obj_hum;
        struct stat sb;
        char *ptr_payload;

        ptr_payload = calloc(1, message->payloadlen+1);
        memcpy(ptr_payload, message->payload, message->payloadlen);
        root = json_tokener_parse(ptr_payload);
        obj_device = json_object_object_get(root, "device");
        obj_temp = json_object_object_get(root, "temp");
        obj_hum = json_object_object_get(root, "hum");

        if (obj_device && obj_temp && obj_hum)
        {
            sprintf(device_name, "%s", json_object_get_string(obj_device));

            sprintf(device_path, "%s/%s", AM2315_PATH, device_name);
            if (stat(device_path, &sb) || !S_ISDIR(sb.st_mode))
            {
                LOG_WARN("device directory [%s] not exist\n", device_path);
                mkdir(device_path, 0755);
            }

            sprintf(datfile_path, "%s/%d", device_path, t_info->tm_year+1900);
            if (stat(datfile_path, &sb) || !S_ISDIR(sb.st_mode))
            {
                LOG_WARN("device directory [%s] not exist\n", datfile_path);
                mkdir(datfile_path, 0755);
            }

            sprintf(datfile_path, "%s/%d/%d.dat", device_path, t_info->tm_year+1900, t_info->tm_mon+1);
            fp = fopen(datfile_path, "a+");
            if (fp)
            {
                //printf("temp: %.2f, hum: %.2f\n", json_object_get_double(obj_temp), json_object_get_double(obj_hum));

                if (!isnan(json_object_get_double(obj_temp)) && !isnan(json_object_get_double(obj_hum)))
                {
                    fseek(fp, 0, SEEK_END);
                    fprintf(fp, "%d/%02d/%02d/%02d:%02d:%02d,%.2f,%.2f\n",
                            t_info->tm_year+1900, t_info->tm_mon+1, t_info->tm_mday, t_info->tm_hour, t_info->tm_min, t_info->tm_sec,
                            json_object_get_double(obj_temp), json_object_get_double(obj_hum));

					check_threshold(json_object_get_double(obj_temp), json_object_get_double(obj_hum));
                }
                else
                {
                    LOG_ERR("Invalid value\n");
                }

                fclose(fp);
            }
            else
            {
                LOG_ERR("can't open data file [%s]\n", datfile_path);
            }

            free(ptr_payload);
        }
    }
    else if (!strncmp(topicName, (char *)cfg_topic_control.u.str_value, strlen((char *)cfg_topic_control.u.str_value)))
    {
        json_object *root;
        char *ptr_payload;
        int cfg_type;
        int cfg_int_val;
        double cfg_double_val;
        const char *cfg_string_val;

        LOG_INFO("Receive Control...\n");
        ptr_payload = calloc(1, message->payloadlen+1);
        memcpy(ptr_payload, message->payload, message->payloadlen);
        root = json_tokener_parse(ptr_payload);

        json_object_object_foreach(root, cfg_item, obj_val)
        {
            cfg_type = json_object_get_type(obj_val);

            switch(cfg_type)
            {
                case json_type_double:
                    cfg_double_val = (double)json_object_get_double(obj_val);
                    set_config_value(cfg_item, (void *)&cfg_double_val);
                    LOG_INFO("Set %s to %.1f\n", cfg_item, cfg_double_val);
                    break;

                case json_type_int:
                    cfg_int_val = (int)json_object_get_double(obj_val);
                    set_config_value(cfg_item, (void *)&cfg_int_val);
                    LOG_INFO("Set %s to %d\n", cfg_item, cfg_int_val);
                    break;

                case json_type_string:
                    cfg_string_val = json_object_get_string(obj_val);
                    set_config_value(cfg_item, (void *)cfg_string_val);
                    LOG_INFO("Set %s to %s\n", cfg_item, cfg_string_val);
                    break;

                default: break;
           }
        }

        free(ptr_payload);
    }

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);

    LOG_DBG("Leave...\n");
    return 1;
}

void connlost(void *context, char *cause)
{
    int rc;
	config_t cfg_client_id, cfg_topic_temp_hum, cfg_topic_control, cfg_qos;

    LOG_DBG("Enter...\n");

    LOG_INFO("Connection lost\n");
    LOG_NO_HEADER("     cause: %s\n", cause);
    
	get_config_value("mqtt_client_id", &cfg_client_id);
	get_config_value("mqtt_topic_temp_hum", &cfg_topic_temp_hum);
	get_config_value("mqtt_topic_control", &cfg_topic_control);
	get_config_value("mqtt_qos", &cfg_qos);

    while ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        LOG_ERR("Failed to connect, return code %d...try again\n", rc);
        sleep(3);
    }

    LOG_INFO("success to connect to broker\n");

    LOG_INFO("Subscribing to topic %s\nfor client"
            " %s using QoS%d\n\n", 
				(char *)cfg_topic_temp_hum.u.str_value, (char *)cfg_client_id.u.str_value, cfg_qos.u.int_value);

    MQTTClient_subscribe(client, cfg_topic_temp_hum.u.str_value, cfg_qos.u.int_value);

    LOG_INFO("Subscribing to topic %s\nfor client"
            " %s using QoS%d\n\n", 
				(char *)cfg_topic_control.u.str_value, (char *)cfg_client_id.u.str_value, cfg_qos.u.int_value);

    MQTTClient_subscribe(client, cfg_topic_control.u.str_value, cfg_qos.u.int_value);

    LOG_DBG("Leave...\n");
}

int load_config(void)
{
	FILE *fp;
	int length;
	char *cfg_ptr;
	json_object *root, *item;
	int cnt;

	fp = fopen(NINANO_CONFIG, "r");
	if (fp == NULL)
	{
		perror("can't open config file");
		return -1;
	}

	fseek(fp, 0, SEEK_END);
	length = ftell(fp);

	cfg_ptr = calloc(1, length+1);

	fseek(fp, 0, SEEK_SET);
	fread(cfg_ptr, length, 1, fp);

	root = json_tokener_parse(cfg_ptr);

	cnt = 0;
	while (g_cfg[cnt].type != CFG_TYPE_END)
	{
		item = json_object_object_get(root, g_cfg[cnt].item_name);
		if (item == NULL)
		{
			LOG_WARN("no found config item [%s]\n", g_cfg[cnt].item_name);
		}
		else
		{
			LOG_INFO("item: %s, ", g_cfg[cnt].item_name);
			if (g_cfg[cnt].type == CFG_TYPE_STRING)
			{
                const char *ptr;

                ptr = json_object_get_string(item);
                set_config_value(g_cfg[cnt].item_name, (void *)ptr);
				LOG_NO_HEADER("value: %s\n", (char *)g_cfg[cnt].u.str_value);
			}
			else if (g_cfg[cnt].type == CFG_TYPE_INT)
			{
                int int_val;

                int_val = json_object_get_double(item);
				set_config_value(g_cfg[cnt].item_name, (void *)&int_val);
				LOG_NO_HEADER("value: %d\n", g_cfg[cnt].u.int_value);
			}
			else if (g_cfg[cnt].type == CFG_TYPE_DOUBLE)
			{
                double double_val;

                double_val = json_object_get_double(item);
				set_config_value(g_cfg[cnt].item_name, (void *)&double_val);
				LOG_NO_HEADER("value: %.2f\n", g_cfg[cnt].u.double_value);
			}
			else
			{
				LOG_NO_HEADER("\n");
				LOG_WARN("unknown config type [%d]\n", g_cfg[cnt].type);
			}
		}

		cnt++;
	}


	fclose(fp);
	free(cfg_ptr);

    fflush(stdout);

	return 0;
}

int main(int argc, char* argv[])
{
    int rc;
    struct stat sb;
	config_t cfg_host_addr, cfg_client_id, cfg_username, cfg_password, cfg_topic_temp_hum, cfg_topic_control, cfg_qos;


	if (load_config() < 0)
	{
		exit(-1);
	}

    if (stat(NINANO_PATH, &sb) || !S_ISDIR(sb.st_mode))
    {
    	LOG_WARN("Ninano directory not exist\n");
    	mkdir(NINANO_PATH, 0755);
    }

    /* AM2315 stuff */
    if (stat(AM2315_PATH, &sb) || !S_ISDIR(sb.st_mode))
    {
    	LOG_WARN("AM2315 directory not exist\n");
    	mkdir(AM2315_PATH, 0755);
    }

	//unlink(NINANO_ALARM_LOCK);

	get_config_value("mqtt_host_addr", &cfg_host_addr);
	get_config_value("mqtt_client_id", &cfg_client_id);
	get_config_value("mqtt_username", &cfg_username);
	get_config_value("mqtt_password", &cfg_password);
	get_config_value("mqtt_topic_temp_hum", &cfg_topic_temp_hum);
	get_config_value("mqtt_topic_control", &cfg_topic_control);
	get_config_value("mqtt_qos", &cfg_qos);

    MQTTClient_create(&client, cfg_host_addr.u.str_value, cfg_client_id.u.str_value,
            MQTTCLIENT_PERSISTENCE_NONE, NULL);

    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    conn_opts.username = cfg_username.u.str_value;
    conn_opts.password = cfg_password.u.str_value;

    MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, delivered);

    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        LOG_ERR("Failed to connect, return code %d\n", rc);
        exit(EXIT_FAILURE);
    }

    LOG_INFO("Subscribing to topic %s\nfor client"
            " %s using QoS%d\n\n", (char *)cfg_topic_temp_hum.u.str_value, (char *)cfg_client_id.u.str_value, cfg_qos.u.int_value);

    MQTTClient_subscribe(client, cfg_topic_temp_hum.u.str_value, cfg_qos.u.int_value);

    LOG_INFO("Subscribing to topic %s\nfor client"
            " %s using QoS%d\n\n", (char *)cfg_topic_control.u.str_value, (char *)cfg_client_id.u.str_value, cfg_qos.u.int_value);

    MQTTClient_subscribe(client, cfg_topic_control.u.str_value, cfg_qos.u.int_value);

    while (1)
    {
        sleep(1);
    }

    MQTTClient_unsubscribe(client, cfg_topic_temp_hum.u.str_value);
    MQTTClient_unsubscribe(client, cfg_topic_control.u.str_value);
    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);

    return rc;
}
