static void on_display(void){
    glClearColor(C_BG.r, C_BG.g, C_BG.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if(!g_vshm){ glutSwapBuffers(); return; }

    int round = g_vshm->round;
    int t1w   = g_vshm->team1_wins;
    int t2w   = g_vshm->team2_wins;
    int winr  = g_vshm->win_rounds;
    int champ = g_vshm->champion;

    draw_header(round, t1w, t2w, winr, champ);

    float header_h = 64.0f;
    float usable_h = (float)g_H - header_h;
    float lane_h   = usable_h / 2.0f;
    float margin   = visual_margin();
    float lw       = (float)g_W - margin * 2.0f;
    float cx       = (float)g_W / 2.0f;

    draw_lane(0, cx, header_h + lane_h * 0.44f,         lw, C_T1, 1);

    glColor3f(0.17f, 0.17f, 0.25f);
    fill_rect(0, header_h + lane_h - 1, (float)g_W, 2);

    draw_lane(1, cx, header_h + lane_h + lane_h * 0.44f, lw, C_T2, 2);

    draw_legend(8, 8);
    if(champ > 0)
        draw_confetti();
    glutSwapBuffers();
}

static void on_reshape(int w, int h){
    g_W = w; g_H = (h > 0 ? h : 1);
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluOrtho2D(0, w, 0, h);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
}

static void on_timer(int v){
    (void)v;
    g_clock += (float)TIMER_MS / 1000.0f;


    if(g_vshm && g_fc > 0){
        if(g_vshm->round != g_seen_round){
            memset(g_anim, 0, sizeof(g_anim));
            team_anim_reset(0);
            team_anim_reset(1);
            g_seen_round = g_vshm->round;
        }
        anim_tick_team(0, shm_t1(g_vshm));
        anim_tick_team(1, shm_t2(g_vshm));
    }

    glutPostRedisplay();
    glutTimerFunc(TIMER_MS, on_timer, 0);
}

static void on_key(unsigned char k, int x, int y){
    (void)x; (void)y;
    if(k == 27 || k == 'q') exit(0);
}




void visual_launch(VisualShm *vshm, int fc, int pipeline_len, int win_rounds){
    pid_t pid = fork();
    if(pid < 0){ perror("visual fork"); return; }
    if(pid > 0) return;   


    vshm->furniture_count = fc;
    vshm->pipeline_len    = pipeline_len;
    vshm->win_rounds      = win_rounds;
    g_vshm = vshm;
    g_fc   = fc;
    g_plen = pipeline_len;
    g_W = 900 + pipeline_len * 46;
    if(g_W < 1140) g_W = 1140;
    if(g_W > 1680) g_W = 1680;
    g_H = (fc > 20) ? 820 : 720;
    g_seen_round = -1;
    memset(g_anim, 0, sizeof(g_anim));
    team_anim_reset(0);
    team_anim_reset(1);

    int    argc = 0;
    char  *av[] = { (char *)"visual", NULL };
    glutInit(&argc, av);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(g_W, g_H);
    glutCreateWindow("Home Furnishing Competition");

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glutDisplayFunc(on_display);
    glutReshapeFunc(on_reshape);
    glutKeyboardFunc(on_key);
    glutTimerFunc(TIMER_MS, on_timer, 0);
    glutMainLoop();
    exit(0);
}
