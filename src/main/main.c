#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "config.h"
#include "pipeline.h"
#include "furniture.h"
#include "visual.h"

int main(int argc, char *argv[]){
    AppSettings settings;

    if (load_settings(argc, argv, &settings) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    int team1_wins = 0;
    int team2_wins = 0;
    int round = 0;

    VisualShm *vshm = visual_shm_create(settings.furniture_pieces);
    visual_launch(vshm, settings.furniture_pieces,
                  settings.child_count, settings.win_rounds);

    while (team1_wins < settings.win_rounds && team2_wins < settings.win_rounds) {
        ++round;

        printf("\n══════════════════════════════════════\n");
        printf("           ROUND %d START\n", round);
        printf("══════════════════════════════════════\n\n");

        furniture_piece *furniture = furniture_init(settings.furniture_pieces, settings.auto_serial);
        if (furniture == NULL) {
            fprintf(stderr, "Failed to initialize furniture\n");
            return EXIT_FAILURE;
        }

        furniture_piece *furniture_t1 = malloc((size_t)settings.furniture_pieces * sizeof(furniture_piece));
        furniture_piece *furniture_t2 = malloc((size_t)settings.furniture_pieces * sizeof(furniture_piece));
        if (furniture_t1 == NULL || furniture_t2 == NULL) {
            perror("malloc");
            free(furniture);
            free(furniture_t1);
            free(furniture_t2);
            return EXIT_FAILURE;
        }

        memcpy(furniture_t1, furniture, (size_t)settings.furniture_pieces * sizeof(furniture_piece));
        memcpy(furniture_t2, furniture, (size_t)settings.furniture_pieces * sizeof(furniture_piece));
        free(furniture);

        furniture_display_table(furniture_t1, settings.furniture_pieces, round);
        fflush(stdout);

        visual_update(vshm, furniture_t1, furniture_t2,
                      settings.furniture_pieces, round,
                      team1_wins, team2_wins, settings.win_rounds);
        furniture_piece *visual_t1 = visual_team_furniture(vshm, 1);
        furniture_piece *visual_t2 = visual_team_furniture(vshm, 2);
        free(furniture_t1);
        free(furniture_t2);
        furniture_t1 = visual_t1;
        furniture_t2 = visual_t2;

        pid_t team1_pid = fork();
        if (team1_pid == -1) {
            perror("fork");
            return EXIT_FAILURE;
        }

        if (team1_pid == 0) {
            setpgid(0, 0);
            int result = run_pipeline(settings.child_count,
                                      furniture_t1,
                                      settings.furniture_pieces,
                                      settings.min_pause,
                                      settings.max_pause,
                                      1,
                                      vshm);
            _exit(result);
        }
        setpgid(team1_pid, team1_pid);

        pid_t team2_pid = fork();
        if (team2_pid == -1) {
            perror("fork");
            kill(team1_pid, SIGTERM);
            waitpid(team1_pid, NULL, 0);
            return EXIT_FAILURE;
        }

        if (team2_pid == 0) {
            setpgid(0, 0);
            int result = run_pipeline(settings.child_count,
                                      furniture_t2,
                                      settings.furniture_pieces,
                                      settings.min_pause,
                                      settings.max_pause,
                                      2,
                                      vshm);
            _exit(result);
        }
        setpgid(team2_pid, team2_pid);

        int status;
        pid_t winner_pid = waitpid(-1, &status, 0);

        int winning_team;
        pid_t loser_pid;

        if (winner_pid == team1_pid) {
            winning_team = 1;
            loser_pid = team2_pid;
            ++team1_wins;
        } else {
            winning_team = 2;
            loser_pid = team1_pid;
            ++team2_wins;
        }

        int losing_team = (winning_team == 1) ? 2 : 1;
        printf("\n[Controller] Team %d finished round %d first.\n", winning_team, round);
        printf("[Controller] Stopping Team %d (pid %d, pgid %d)...\n",
               losing_team, loser_pid, loser_pid);
        fflush(stdout);

        kill(-loser_pid, SIGTERM);
        waitpid(loser_pid, NULL, 0);

        printf("[Controller] Team %d stopped. All child processes reaped.\n", losing_team);
        fflush(stdout);

        printf("\n══════════════════════════════════════\n");
        printf("    ROUND %d WINNER: Team %d\n", round, winning_team);
        printf("    Team 1: %d wins | Team 2: %d wins\n", team1_wins, team2_wins);
        printf("══════════════════════════════════════\n");
    }

    int champion = (team1_wins >= settings.win_rounds) ? 1 : 2;

    printf("\n═══════════════════════════════════════\n");
    printf("  COMPETITION WINNER: TEAM %d (%d-%d)\n",
           champion, team1_wins, team2_wins);
    printf("═══════════════════════════════════════\n\n");

    visual_set_champion(vshm, champion);
    sleep(4);
    visual_destroy(vshm, settings.furniture_pieces);

    return EXIT_SUCCESS;
}
