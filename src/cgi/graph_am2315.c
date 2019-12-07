#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <time.h>

#include "cgic.h"

#define AM2315_PATH			"/var/ninano/AM2315"
#define MAX_DEVICE			64
#define NINANO_SINCE_YEAR	2019


int comparisonFunctionString(const void *a, const void *b)
{
	return( strcmp( (char *)a, (char *)b) );
}

int loadTemperatureHummidity(char *device_name, FILE *fp, int index)
{
	int nr;
	int year, month, day, hour, minute, second;
	float temperature, hummidity;
	
	fprintf(cgiOut, 
			"		temperature_data[%d] = new google.visualization.DataTable();\n"
			"		temperature_data[%d].addColumn('date', 'Month');\n"
			"		temperature_data[%d].addColumn('number', \"온도\");\n"
			"		temperature_data[%d].addRows([\n", index, index, index, index);

	while ((nr = fscanf(fp, "%d/%d/%d/%d:%d:%d,%f,%f\n", 
			&year, &month, &day, &hour, &minute, &second, &temperature, &hummidity)) != EOF)
	{
		fprintf(cgiOut,
				"				[new Date('%d/%d/%d/%d:%d:%d'), %.2f],\n", year, month, day, hour, minute, second, temperature);
	}

	fprintf(cgiOut, 
			"			  ]);\n"
			);

	fseek(fp, 0, SEEK_SET);
	
	fprintf(cgiOut, 
			"		hummidity_data[%d] = new google.visualization.DataTable();\n"
			"		hummidity_data[%d].addColumn('date', 'Month');\n"
			"		hummidity_data[%d].addColumn('number', \"습도\");\n"
			"		hummidity_data[%d].addRows([\n", index, index, index, index);

	while ((nr = fscanf(fp, "%d/%d/%d/%d:%d:%d,%f,%f\n", 
			&year, &month, &day, &hour, &minute, &second, &temperature, &hummidity)) != EOF)
	{
		fprintf(cgiOut,
				"				[new Date('%d/%d/%d/%d:%d:%d'), %.2f],\n", year, month, day, hour, minute, second, hummidity);
	}

	fprintf(cgiOut, 
			"			  ]);\n"
			);
	

	fprintf(cgiOut, 
			"		temperature_option[%d] = {\n"
			"			title: '식물배양실 온도 그래프 (%s) - 현재 온도 %.1f ℃',\n"
			"			width: 900,\n"
			"			height: 500,\n"
			"			// Gives each series an axis that matches the vAxes number below.\n"
			"			series: {\n"
			"				0: {targetAxisIndex: 0},\n"
			"			},\n"
			"			vAxes: {\n"
			"				// Adds titles to each axis.\n"
			"				0: {title: '온도 (℃)', minValue: 10, maxValue: 40},\n"
			"			},\n"
			"			vAxis: {\n"
			"				//minValue: 0\n"
			"			},\n"
			"			explorer: {\n"
			"				actions: ['dragToZoom', 'rightClickToReset'],\n"
			"				axis: 'horizontal',\n"
			"				keepInBounds: true,\n"
			"				maxZoomIn: 60.0\n"
			"			},\n"
			"			colors:['red','#004411']\n"
			"		};\n", index, device_name, temperature);
	fprintf(cgiOut,
			"\n"
			);
	fprintf(cgiOut,
			"		hummidity_option[%d] = {\n"
			"			title: '식물배양실 습도 그래프 (%s) - 현재 습도 %.1f %',\n"
			"			width: 900,\n"
			"			height: 500,\n"
			"			// Gives each series an axis that matches the vAxes number below.\n"
			"			series: {\n"
			"				0: {targetAxisIndex: 0},\n"
			"			},\n"
			"			vAxes: {\n"
			"				// Adds titles to each axis.\n"
			"				0: {title: '습도 (%)', minValue: 0, maxValue: 100},\n"
			"			},\n"
			"			vAxis: {\n"
			"				//minValue: 0\n"
			"			},\n"
			"			explorer: {\n"
			"				actions: ['dragToZoom', 'rightClickToReset'],\n"
			"				axis: 'horizontal',\n"
			"				keepInBounds: true,\n"
			"				maxZoomIn: 60.0\n"
			"			},\n"
			"		};\n", index, device_name, hummidity);
	fprintf(cgiOut,
			"\n"
			);
	
	return 0;
}

void drawTemperatureGraph(int max_count)
{
	int i;
	
	fprintf(cgiOut,
			"		function drawTemperatureLineChart() {\n"
			);
	
	for (i = 0; i < max_count; i++)
	{
		fprintf(cgiOut,
				"			var temperatureChart%d = new google.visualization.LineChart(document.getElementById('chart_div%d'));\n"
				"			temperatureChart%d.draw(temperature_data[%d], temperature_option[%d]);\n\n", i, i, i, i, i);
	}
	
	fprintf(cgiOut,
			"		}\n\n"
			);
}

void drawHummidityGraph(int max_count)
{
	int i;
	
	fprintf(cgiOut,
			"		function drawHummidityLineChart() {\n"
			);
	
	for (i = 0; i < max_count; i++)
	{
		fprintf(cgiOut,
				"			var hummidityChart%d = new google.visualization.LineChart(document.getElementById('chart_div%d'));\n"
				"			hummidityChart%d.draw(hummidity_data[%d], hummidity_option[%d]);\n\n", i, i, i, i, i);
	}
	
	fprintf(cgiOut,
			"		}\n\n"
			);
}

void onChangeGraph()
{
	fprintf(cgiOut,
			"		function onChangeGraph() {\n"
			"			var value = graphSel.options[graphSel.selectedIndex].value;\n"
			"\n"
			"			if (value == 'temperature')\n"
			"				drawTemperatureLineChart();\n"
			"			else\n"
			"				drawHummidityLineChart();\n"
			"		}\n\n"
			);
}

int cgiMain(void)
{
	char file_name[64] = { '\0', };
	char device_name[MAX_DEVICE][16] = { '\0', };
	FILE *fp = NULL;
	struct dirent *de;
	time_t rawtime;
	struct tm *t_info;
	int index = 0, count;
	int tot_device = 0, tot_year = 0;
	char str_years[20][8] = { '\0', };
	char *ptr_years[20] = { NULL, };
	char str_sel_year[8] = { '\0', }, str_sel_month[4] = { '\0', };
	const char *str_month[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12"};
	

	time(&rawtime);
	t_info = localtime(&rawtime);
	
	for (count = (t_info->tm_year+1900); count >= NINANO_SINCE_YEAR; count--)
	{
		sprintf(str_years[tot_year], "%d", count);
		ptr_years[tot_year] = str_years[tot_year];
		tot_year++;
	}
	
	/* Send the content type, letting the browser know this is HTML */
	cgiHeaderContentType("text/html");
	
	// opendir() returns a pointer of DIR type.  
	DIR *dr = opendir(AM2315_PATH); 

	if (dr == NULL)  // opendir returns NULL if couldn't open directory 
	{
		printf("Could not open current directory" );
		
		/* TODO */
		return 0;
	}
    

	while ((de = readdir(dr)) != NULL)
	{
		if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
			continue;
	
		strcpy(device_name[tot_device++], de->d_name);
	}
	
	closedir(dr);
	
	qsort((void *)device_name, tot_device, sizeof(device_name[0]), comparisonFunctionString);
	
	fprintf(cgiOut, 
			"<html>\n"
			"<head>\n"
			"	<script type=\"text/javascript\" src=\"https://www.gstatic.com/charts/loader.js\"></script>\n"
			"	<script type=\"text/javascript\">\n"
			"		google.charts.load('current', {'packages':['corechart']});\n"
			"		google.charts.setOnLoadCallback(drawChart);\n"
			"\n"
			"\n"
			"	var temperature_data = new Array();\n"
			"	var hummidity_data = new Array();\n"
			"	var temperature_option = new Array();\n"
			"	var hummidity_option = new Array();\n"
			);
	

	fprintf(cgiOut, 
			"	function drawChart() {\n"
			"		var graphSel = document.getElementById('id_graph_type');\n"
			"		graphSel.onchange = onChangeGraph;\n\n"
			);

	for (count = 0; count < tot_device; count++)
	{
		if (cgiFormSubmitClicked("duration") == cgiFormSuccess)
		{
			int sel_year, sel_month;
			
			cgiFormSelectSingle("year", (char **)ptr_years, tot_year, &sel_year, 0);
			cgiFormSelectSingle("month", (char **)str_month, 12, &sel_month, 0);
			
			strcpy(str_sel_year, ptr_years[sel_year]);
			strcpy(str_sel_month, str_month[sel_month]);

		}
		else
		{
			time(&rawtime);
			t_info = localtime(&rawtime);

			sprintf(str_sel_year, "%d", t_info->tm_year+1900);
			sprintf(str_sel_month, "%d", t_info->tm_mon+1);
		}
		
		snprintf(file_name, sizeof(file_name),
				"%s/%s/%s/%s.dat", AM2315_PATH, device_name[count], str_sel_year, str_sel_month);

		fp = fopen(file_name, "r");
		if (fp)
		{
			loadTemperatureHummidity(device_name[count], fp, index++);
			
			fclose(fp);
		}
		else
		{
		}
	}
	

	drawTemperatureGraph(index);
	drawHummidityGraph(index);
	onChangeGraph();
	
	fprintf(cgiOut,
			"		drawTemperatureLineChart();\n"
			);
	
	fprintf(cgiOut, 
			"	}\n\n");

	fprintf(cgiOut,	
			"	</script>\n"
			"</head>\n");
	
	fprintf(cgiOut,	
			"<body>\n"
			);

	fprintf(cgiOut,	
			"	<form method=\"POST\" enctype=\"multipart/form-data\" action=\""
			);
	cgiValueEscape(cgiScriptName);
	fprintf(cgiOut, "\">\n");
	fprintf(cgiOut,	
			"		<p>"
			"		그래프 선택"
			"		<select id=\"id_graph_type\" name=\"graph_type\">\n"
			"			<option value=\"temperature\">온도</option>\n"
			"			<option value=\"hummidity\">습도</option>\n"
			"		</select>\n"
			"		</p>\n"
			);
	
	fprintf(cgiOut,
			"		<p>\n"
			"		기간 선택  년도\n"
			"		<select name=\"year\">\n"
			);
	for (count = 0; count < tot_year; count++)
	{
		fprintf(cgiOut,
				"			<option value=\"%s\"%s>%s</option>\n",
				ptr_years[count], !strcmp(ptr_years[count], str_sel_year)?" selected":"", ptr_years[count]);
	}
	fprintf(cgiOut,
			"		</select>\n"
			);
	fprintf(cgiOut,
			"			월\n"
			"		<select name=\"month\">\n"
			);
	for (count = 0; count < 12; count++)
	{
		fprintf(cgiOut,
				"			<option value=\"%s\"%s>%s</option>\n",
				str_month[count], !strcmp(str_month[count], str_sel_month)?" selected":"", str_month[count]
			   );
	}
	fprintf(cgiOut,
		"		</select>"
		"		<input type=\"submit\" name=\"duration\" value=\"검색\">\n"
		"		</p>\n"
		);

	fprintf(cgiOut,
			"		<p>"
			"		왼쪽 마우스 버튼으로 Drag하여 범위 지정 후 확대"
			"		</p>"
			"		<p>"
			"		오른쪽 마우스 버튼 클릭으로 원래 상태로 돌아감"
			"		</p>"
			"	</form>\n"
			);
	
	for (count = 0; count < index; count++)
	{
		fprintf(cgiOut,
				"	<div id=\"chart_div%d\"></div>\n"
				"	<br>", count);
	}

	fprintf(cgiOut,
			"</body>\n"
			"</html>\n"
			);

	return 0;
}
