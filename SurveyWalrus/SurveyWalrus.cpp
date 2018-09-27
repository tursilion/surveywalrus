// SurveyWalrus.cpp : Defines the entry point for the console application.
//
// Replacement for SurveyMonkey, more or less
//
// One file of projects
// One file of systems
// Each project has a voting file
// Each system has a column in the voting file (missing columns mean 0)
//
// Commands are:
// add_system <name> <description>
// add_project <name> <description>
// dump_projects
// dump_systems
// get_scores [html]
// add_vote <ip_address>, then on STDIN -> <project_name or index#> <score> <score>...
// generate_table

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>

#define LOCK "lockfile"
#define SYSTEMS "systems.txt"
#define PROJECTS "projects.txt"
#define PREFIX "project%03d.txt"
#define IPFILE "ipfile.txt"
int lock = -1;

#ifdef WIN32
#include <Windows.h>
#include <io.h>
#define open _open
#define close _close
#define unlink _unlink
#define strcasecmp _stricmp
#define S_IRWXU (_S_IREAD|_S_IWRITE)
#else
#include <unistd.h>
#endif

// list of projects
#define MAX_ENTITIES 256
char projects[MAX_ENTITIES][128];
char systems[MAX_ENTITIES][128];

// scoring in a struct so I can qsort
struct _scores {
    char name[128];
    int scoring[MAX_ENTITIES];  // projects to systems, 1 point each (256k!)
    int ranking;                // projects - weighted score
} scores[MAX_ENTITIES];

int getLock() {
    lock = open(LOCK, O_CREAT|O_EXCL, S_IRWXU);
    if (-1 == lock) {
        return 0;
    } else {
        return 1;
    }
}

void unlock() {
    close(lock);
    int cnt = 0;
    int err = 0;
    while ((unlink(LOCK) == -1) && (errno == EBUSY)) {
        ++cnt;
        err = errno;
    }
    if ((errno == EACCES)||(errno==EPERM)) {
        printf("Permission denied to remove lock file!\n");
    } else if (cnt > 0) {
        printf("Unlink took %d cycles, last err %d\n", cnt, err);
    }
}

void delay(int ms) {
#ifdef WIN32
    Sleep(ms);
#else
    usleep(ms*1000);
#endif
}

void strip(char *buf) {
    // strip whitespace from beginning and end
    int len = strlen(buf);
    while ((buf[0])&&(buf[0] <= ' ')) {
        memmove(buf, buf+1, len);   // includes NUL
        --len;
    }
    while ((len>0)&&(buf[len-1] <= ' ')) {
        buf[len-1]='\0';
        --len;
    }
}

void add_system(int argc, char *argv[]) {
    // add_system <name> <description>
    if (argc < 3) {
        printf("You need to pass a name and at least one word of description.\n");
        return;
    }

    FILE *fp = fopen(SYSTEMS, "a");
    if (NULL == fp) {
        printf("Couldn't open SYSTEMS file, code %d\n", errno);
        return;
    }

    fprintf(fp, "%s", argv[2]);
    for (int idx = 3; idx<argc; ++idx) {
        fprintf(fp, " %s", argv[idx]);
    }
    fprintf(fp, "\n");
    fclose(fp);
}

void add_project(int argc, char *argv[]) {
    // add_project <name> <description>
    if (argc < 3) {
        printf("You need to pass a name and at least one word of description.\n");
        return;
    }

    if (isdigit(argv[2][0])) {
        printf("No. Projects can't start with numbers.\n");
        return;
    }

    FILE *fp = fopen(PROJECTS, "a");
    if (NULL == fp) {
        printf("Couldn't open PROJECTS file, code %d\n", errno);
        return;
    }

    fprintf(fp, "%s", argv[2]);
    for (int idx = 3; idx<argc; ++idx) {
        fprintf(fp, " %s", argv[idx]);
    }
    fprintf(fp, "\n");
    fclose(fp);
}

void dump_projects(int argc, char *argv[]) {
    // dump_projects
    // this is just to have a lock-safe way to dump the values for the script
    FILE *fp = fopen(PROJECTS, "r");
    if (NULL == fp) {
        printf("Couldn't open PROJECTS file, code %d\n", errno);
        return;
    }

    while (!feof(fp)) {
        char buf[128];

        if (NULL == fgets(buf, sizeof(buf), fp)) {
            break;
        }
        strip(buf);

        printf("%s\n", buf);
    }

    fclose(fp);
}

void dump_systems(int argc, char *argv[]) {
    // dump_systems
    // this is just to have a lock-safe way to dump the values for the script
    FILE *fp = fopen(SYSTEMS, "r");
    if (NULL == fp) {
        printf("Couldn't open SYSTEMS file, code %d\n", errno);
        return;
    }

    while (!feof(fp)) {
        char buf[128];

        if (NULL == fgets(buf, sizeof(buf), fp)) {
            break;
        }
        strip(buf);

        printf("%s\n", buf);
    }

    fclose(fp);
}

int load_projects() {
    int cnt = 0;

    memset(projects, 0, sizeof(projects));
    FILE *fp = fopen(PROJECTS, "r");
    if (NULL == fp) {
        printf("Couldn't open PROJECTS file, code %d\n", errno);
        return 0;
    }

    while (!feof(fp)) {
        char buf[128];

        if (NULL == fgets(buf, sizeof(buf), fp)) {
            break;
        }
        strip(buf);

        char *p = strchr(buf, ' ');
        if (NULL == p) {
            printf("- Missing project name in config file\n");
            continue;
        }
        *p='\0';
        buf[127]='\0';  // be safe
        memcpy(projects[cnt++], buf, 128);
        if (cnt >= MAX_ENTITIES-1) {
            printf("- too many projects, truncating!\n");
            break;
        }
    }

    fclose(fp);

    return cnt;
}

int load_systems() {
    int cnt = 0;

    memset(systems, 0, sizeof(systems));
    FILE *fp = fopen(SYSTEMS, "r");
    if (NULL == fp) {
        printf("Couldn't open SYSTEMS file, code %d\n", errno);
        return 0;
    }

    while (!feof(fp)) {
        char buf[128];

        if (NULL == fgets(buf, sizeof(buf), fp)) {
            break;
        }
        strip(buf);

        char *p = strchr(buf, ' ');
        if (NULL == p) {
            printf("- Missing system name in config file\n");
            continue;
        }
        *p='\0';
        buf[127]='\0';  // be safe
        memcpy(systems[cnt++], buf, 128);
        if (cnt >= MAX_ENTITIES-1) {
            printf("- too many systems, truncating!\n");
            break;
        }
    }

    fclose(fp);

    return cnt;
}

void add_vote(int argc, char *argv[]) {
    // add_vote <ip_address>
    // reads from STDIN, lines of: <project_name> <score> <score>...
    // we need to find the project in the input and use that to update the
    // projects file. To ensure we get them all, we'll read in a list
    // first and then we can fill in the others with zeros
    char buf[128];
    char fn[128];
    int cnt, proj;

    // first check whether this IP is already in use
    // since we can't trust the input, we keep only the digits
    if (argc < 3) {
        // no ip address, ignore the input
        printf("You didn't say where you are calling from!\n");
        return;
    }
    // reuse fn and buf for this...
    strncpy(buf, argv[2], sizeof(buf));
    buf[sizeof(buf)-1]='\0';
    for (unsigned int idx=0; idx<strlen(buf); ++idx) {
        if (buf[idx]=='.') {
            // don't keep any punctuation, but
            // if we don't keep a separator then
            // 12.1 and 1.12 will look the same ;)
            buf[idx]='q';
        } else if ((buf[idx]<'0')||(buf[idx]>'9')) {
            // delete the character
            memcpy(&buf[idx], &buf[idx+1], strlen(&buf[idx]));
            --idx;  // to account for the pending increment
        }
    }
    if (strlen(buf) < 1) {
        // no ip address, ignore the input
        printf("You didn't say where you are calling from!!\n");
        return;
    }
    FILE *fp = fopen(IPFILE, "r");
    if (NULL != fp) {
        // we have one, so scan it
        while (!feof(fp)) {
            if (NULL == fgets(fn, sizeof(fn), fp)) {
                break;
            }
            strip(fn);
            if (0 == strcmp(fn, buf)) {
                // already voted today... ignore the input
                printf("<!- already voted ->\n");
                fclose(fp);
                return;
            }
        }
        fclose(fp);
    }
    // now update it
    fp=fopen(IPFILE, "a");
    fprintf(fp, "%s\n", buf);
    fclose(fp);

    // now go ahead and process the passed in votes
    cnt = load_projects();

    while (fgets(buf, sizeof(buf), stdin) != NULL) {
        char *p;
        strip(buf);

        p = strchr(buf, ' ');   // find the first space
        if (p == NULL) {
            printf("- Missing project name\n");
            continue;
        }
        *p = '\0';

        if (isdigit(buf[0])) {
            proj = atoi(buf);
            if (proj >= cnt) {
                printf("Bad index passed. No.\n");
                // just stop here
                break;
            }
        } else {
            // find the project in the list
            proj = -1;
            for (int idx=0; idx<cnt; ++idx) {
                if (0 == strcasecmp(projects[idx], buf)) {
                    proj = idx;
                    break;
                }
            }
            if (proj == -1) {
                printf("Couldn't find project '%s' to update\n", buf);
                continue;
            }
        }

        // update the passed in file with the line provided
        sprintf(fn, PREFIX, proj);
        FILE *fp = fopen(fn, "a");
        if (NULL == fp) {
            printf("Can't open work file %s, code %d", fn, errno);
        } else {
            fprintf(fp, "%s\n", p+1);
            fclose(fp);
        }

        // flag this file as done
        projects[proj][0]='\0';
    }

    // now check if we missed any projects, and set zero on them
    // this is to ensure that if the project gets a new vote, it's not
    // worth less than another project's new vote just because someone
    // else didn't vote for it. It keeps the weighting balanced.
    //
    // I'm still undecided on new projects getting similar balancing,
    // I think maybe not since it gives older projects some chance
    // at being higher scored.
    for (int idx=0; idx<cnt; ++idx) {
        if (projects[idx][0] != '\0') {
            sprintf(buf, PREFIX, idx);
            FILE *fp = fopen(buf, "a");
            if (NULL == fp) {
                printf("Can't open work file %s, code %d", buf, errno);
            } else {
                fprintf(fp, "0\n");
                fclose(fp);
            }
        }
    }
}

// qsort helper function
int sortscore(const void *a, const void *b) {
    // struct _scores {
    //     int scoring[MAX_ENTITIES];  // projects to systems, 1 point each (256k!)
    //     int ranking;                // projects - weighted score
    // } scores[MAX_ENTITIES];
    return ((struct _scores *)b)->ranking - ((struct _scores *)a)->ranking;
}

void get_scores(int argc, char *argv[]) {
    // get_scores
    // the weighting assumes that all projects are scored at the same time
    int num_projects, num_systems;
    int isHtml = 0;
    if ((argc > 2)&&(0 == strcmp(argv[2],"html"))) {
      isHtml = 1;
    }

    // get labels into memory
    // so we need to read the list of projects first
    num_projects = load_projects();
    printf("Loaded %d projects...\n", num_projects);

    // then we read in the list of systems
    num_systems = load_systems();
    printf("Loaded %d systems...\n", num_systems);

    if (isHtml) printf("<br>\n");

    // we need a 2D array - projects to systems. 
    // struct _scores {
    //     int scoring[MAX_ENTITIES];  // projects to systems, 1 point each (256k!)
    //     int ranking;                // projects - weighted score
    // } scores[MAX_ENTITIES];
    memset(scores, 0, sizeof(scores));

    // Then we read each project
    // and score it up, and add up the score for each system.
    // Newer stuff has fewer lines, so will automatically end up with a lower score...
    for (int proj = 0; proj < num_projects; ++proj) {
        char buf[128];

	if (isHtml) {
		printf("<!-- Checking '%s'... -->\n", projects[proj]);
	} else {
		printf("Checking '%s'...\n", projects[proj]);
	}
        memcpy(scores[proj].name, projects[proj], 128);

        sprintf(buf, PREFIX, proj);
        FILE *fp = fopen(buf, "r");
        if (NULL == fp) {
            if (isHtml) {
              printf("<!-- Failed to open '%s' for project '%s', code %d -->\n", buf, projects[proj], errno);
	    } else {
              printf("Failed to open '%s' for project '%s', code %d\n", buf, projects[proj], errno);
	    }
            continue;
        }

        int line = 0;
        while (NULL != fgets(buf, sizeof(buf), fp)) {
            // each space-separated value should be a system score
            // there may be fewer scores than systems, the rest are just zero
            // if there's any score at all, this row also scores a ranking
            int score = 0;
            int sys = 0;

            strip(buf);
            ++line;

            char *p = buf;
            while ((p)&&(*p)) {
                if (atoi(p)) {
                    // any value is '1'
                    score=line;
                    scores[proj].scoring[sys]++;
                }
                p=strchr(p, ' ');
                while ((p)&&(*p)&&(*p <= ' ')) ++p;
                ++sys;
            }
            scores[proj].ranking += score;
        }
    }

    // Sort the array by system total
    if (isHtml) {
        printf("<!-- Sorting... -->\n");
    } else {
        printf("Sorting...\n");
    }
    qsort(scores, num_projects, sizeof(scores[0]), sortscore);

    int max = scores[0].ranking;
    if (max == 0) {
        printf("No scores yet!\n");
        return;
    }

    // And finally, output the list: 

    if (isHtml) {
        // html output
        printf("<table>\n");
        printf("<tr>\n");
        printf("<th class=\"align_left\">Project</th> <th>Score</th>\n");
        for (int idx=0; idx<num_systems; ++idx) {
            char *p = systems[idx] + strlen(systems[idx]) + 1;
            if (p > systems[idx]+127) p = "";
            printf("<th><div class=\"system\">%s\n", systems[idx]);
            printf("  <span class=\"tooltiptext\">%s</span>\n", p);
            printf("</div></th>\n");
        }
        printf("</tr>\n\n");

        for (int idx=0; idx<num_projects; ++idx) {
            printf("<tr>\n");
            char *p = scores[idx].name + strlen(scores[idx].name) + 1;
            if (p > scores[idx].name+127) p = "";
            printf("<td><div class=\"tooltip\">%s", scores[idx].name);
            printf("<span class=\"tooltiptext\">%s</span>", p);
            printf("</div></td>");
            printf("<td>%5d</td> ", scores[idx].ranking*1000/max);
            for (int i2=0; i2<num_systems; ++i2) {
                printf("<td>%5d</td> ", scores[idx].scoring[i2]);
            }
            printf("</tr>\n");
        }
        printf("</table>\n");

    } else {
        // text output
        // Title line <empty> <system1> <system2> <system3>...
        // Project <score> <score> <score> <total score>
        printf("\n");
        printf("Project    Score ");
        for (int idx=0; idx<num_systems; ++idx) {
            printf("%5.5s ", systems[idx]);
        }
        printf("\n\n");
        for (int idx=0; idx<num_projects; ++idx) {
            printf("%-10.10s ", scores[idx].name);
            printf("%5d ", scores[idx].ranking*1000/max);
            for (int i2=0; i2<num_systems; ++i2) {
                printf("%5d ", scores[idx].scoring[i2]);
            }
            printf("\n");
        }
        printf("\n");
    }
}

// generate the HTML form with all the checkboxes needed
void generate_table(int argc, char *argv[]) {
    int num_projects, num_systems;

    // get labels into memory
    // so we need to read the list of projects first
    num_projects = load_projects();
    printf("Loaded %d projects...\n", num_projects);

    // then we read in the list of systems
    num_systems = load_systems();
    printf("Loaded %d systems...\n", num_systems);

    // html output
    printf("<form action=\"/cgi-bin/walrusscore.cgi\" method=\"post\">\n");
    printf("<table border=\"1\" id=\"mytable\">\n");
    printf("<tr>\n");
    printf("<th class=\"align_left\" onclick=\"toggleColumn()\">Project</th> \n");
    printf("<th class=\"col_desc\" onclick=\"toggleColumn()\">Description (click to hide)</th> \n");
    for (int idx=0; idx<num_systems; ++idx) {
        char *p = systems[idx] + strlen(systems[idx]) + 1;
        if (p > systems[idx]+127) p = "";
        printf("<th><div class=\"tooltip\">%s\n", systems[idx]);
        printf("  <span class=\"tooltiptext\">%s</span>\n", p);
        printf("</div></th>\n");
    }
    printf("</tr>\n\n");

    for (int idx=0; idx<num_projects; ++idx) {
        printf("<tr>\n");
        char *p = projects[idx] + strlen(projects[idx]) + 1;
        if (p > projects[idx]+127) p = "";
        printf("<td><div class=\"tooltip\">%s", projects[idx]);
        printf("<span class=\"tooltiptext\">%s</span>", p);
        printf("</div></td>");
      	// reprint the description in a hidable column - tooltip still works
	printf("<td class=\"col_desc\">%s</td>", p);
        for (int i2=0; i2<num_systems; ++i2) {
            printf("<td class=\"align_center\"><input type=\"checkbox\" name=\"%04d_%04d\" value=\"1\"> </td> ", idx, i2);
        }
        printf("</tr>\n");
    }
    printf("</table>\n");
    printf("<br><input class=\"button_submit\" type=\"submit\" value=\"Submit\">\n");
    printf("</form>\n");
}

int main(int argc, char *argv[])
{
    // set the working directory
#ifdef WIN32
    // this is only for debug
    SetCurrentDirectory("D:\\work\\SurveyWalrus\\sw");
#else
    if (chdir("/home/tursilion/surveywalrus/sw")) {
        printf("Can't get to working directory. Code %d\n", errno);
        return 1;
    }
#endif

    // acquire the lockfile
    int cnt = 100;
    while (!getLock()) {
        printf("... waiting for lock...\n");
        delay(500);
        if (--cnt < 1) {
            printf("timing out, sorry!\n");
            return 1;
        }
    }

    // do what we're told to do
    if (argc<2) {
        printf("surveywalrus add_system <name> <description>\n");
        printf("surveywalrus add_project <name> <description>\n");
        printf("surveywalrus dump_projects\n");
        printf("surveywalrus dump_systems\n");
        printf("surveywalrus get_scores [html]\n");
        printf("surveywalrus add_vote (STDIN lines: <project_name or index> <score> <score>...)\n");
        printf("surveywalrus generate_table\n");
    } else if (0 == strcmp(argv[1], "add_system")) {
        add_system(argc, argv);
    } else if (0 == strcmp(argv[1], "add_project")) {
        add_project(argc, argv);
    } else if (0 == strcmp(argv[1], "dump_projects")) {
        dump_projects(argc, argv);
    } else if (0 == strcmp(argv[1], "dump_systems")) {
        dump_systems(argc, argv);
    } else if (0 == strcmp(argv[1], "get_scores")) {
        get_scores(argc, argv);
    } else if (0 == strcmp(argv[1], "add_vote")) {
        add_vote(argc, argv);
    } else if (0 == strcmp(argv[1], "generate_table")) {
        generate_table(argc, argv);
    } else {
        printf("I've no idea what you asked for.\n");
    }

    unlock();
    return 0;
}
