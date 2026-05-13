typedef struct { float r, g, b; } Col;

static void glcol (Col c)           { glColor3f(c.r, c.g, c.b); }
static void glcol4(Col c, float a)  { glColor4f(c.r, c.g, c.b, a); }

static const Col C_BG    = {0.07f, 0.07f, 0.11f};
static const Col C_FLOOR = {0.20f, 0.20f, 0.28f};
static const Col C_PIPE  = {0.25f, 0.25f, 0.35f};
static const Col C_HDR   = {0.09f, 0.09f, 0.17f};
static const Col C_T1    = {0.35f, 0.62f, 0.95f};
static const Col C_T2    = {0.90f, 0.40f, 0.25f};

static Col status_col(piece_status s){
    switch(s){
        case AVAILABLE:       return (Col){0.55f, 0.55f, 0.62f};
        case MOVING_FORWARD:  return (Col){0.18f, 0.85f, 0.35f};
        case MOVING_BACKWARD: return (Col){0.95f, 0.42f, 0.10f};
        case PLACED:          return (Col){0.22f, 0.55f, 0.98f};
        case BLOCKED:         return (Col){0.88f, 0.15f, 0.18f};
        default:              return (Col){0.50f, 0.50f, 0.50f};
    }
}

static void fill_rect(float x,float y,float w,float h){
    glBegin(GL_QUADS);
    glVertex2f(x,y); glVertex2f(x+w,y); glVertex2f(x+w,y+h); glVertex2f(x,y+h);
    glEnd();
}
static void stroke_rect(float x,float y,float w,float h){
    glBegin(GL_LINE_LOOP);
    glVertex2f(x,y); glVertex2f(x+w,y); glVertex2f(x+w,y+h); glVertex2f(x,y+h);
    glEnd();
}
static void fill_circle(float cx,float cy,float r,int n){
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx,cy);
    for(int i=0;i<=n;i++){
        float a=(float)i/(float)n*TAU;
        glVertex2f(cx+cosf(a)*r, cy+sinf(a)*r);
    }
    glEnd();
}
static void fill_ellipse(float cx,float cy,float rx,float ry,int n){
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx,cy);
    for(int i=0;i<=n;i++){
        float a=(float)i/(float)n*TAU;
        glVertex2f(cx+cosf(a)*rx, cy+sinf(a)*ry);
    }
    glEnd();
}
static void stroke_circle(float cx,float cy,float r,int n){
    glBegin(GL_LINE_LOOP);
    for(int i=0;i<n;i++){
        float a=(float)i/(float)n*TAU;
        glVertex2f(cx+cosf(a)*r, cy+sinf(a)*r);
    }
    glEnd();
}
static void draw_line(float x1,float y1,float x2,float y2){
    glBegin(GL_LINES); glVertex2f(x1,y1); glVertex2f(x2,y2); glEnd();
}
static void draw_limb(float x1,float y1,float x2,float y2,float radius,Col c){
    glcol(c);
    glLineWidth(radius * 2.0f);
    draw_line(x1, y1, x2, y2);
    fill_circle(x1, y1, radius, 12);
    fill_circle(x2, y2, radius, 12);
}
static void bstr(float x,float y,const char *s,void *font){
    glRasterPos2f(x,y);
    for(;*s;s++) glutBitmapCharacter(font,*s);
}









static void draw_worker(float cx, float fy, Col body, int role, int carrying){
    const float LEG = 24.0f;
    const float TORSO = 26.0f;
    const float NECK = 5.5f;
    const float HR = 10.5f;

    float ankle_y = fy + 2.0f;
    float knee_y  = fy + LEG * 0.55f;
    float hip_y   = fy + LEG;
    float sh_y    = hip_y + TORSO;
    float neck_y  = sh_y + NECK;
    float head_y  = neck_y + HR - 1.0f;

    Col skin  = {0.94f, 0.72f, 0.52f};
    Col hair  = {0.11f, 0.08f, 0.06f};
    Col pants = {0.10f, 0.16f, 0.25f};
    Col shoe  = {0.04f, 0.04f, 0.05f};
    Col shirt = {0.92f, 0.92f, 0.86f};
    Col overalls = {fminf(body.r * 0.82f, 1.0f),
                    fminf(body.g * 0.82f, 1.0f),
                    fminf(body.b * 0.82f, 1.0f)};
    Col trim = {1.0f, 0.82f, 0.22f};


    glColor4f(0,0,0,0.18f);
    fill_ellipse(cx, fy - 1.0f, 16.0f, 4.5f, 18);


    draw_limb(cx - 4.9f, hip_y, cx - 7.6f, knee_y, 3.5f, pants);
    draw_limb(cx - 7.6f, knee_y, cx - 9.8f, ankle_y, 3.5f, pants);
    draw_limb(cx + 4.9f, hip_y, cx + 7.6f, knee_y, 3.5f, pants);
    draw_limb(cx + 7.6f, knee_y, cx + 9.8f, ankle_y, 3.5f, pants);
    glcol(shoe);
    fill_ellipse(cx - 11.4f, fy, 7.2f, 2.9f, 12);
    fill_ellipse(cx + 11.4f, fy, 7.2f, 2.9f, 12);


    glcol(shirt);
    glBegin(GL_POLYGON);
    glVertex2f(cx - 12.5f, sh_y - 1.0f);
    glVertex2f(cx + 12.5f, sh_y - 1.0f);
    glVertex2f(cx + 9.4f, hip_y - 1.5f);
    glVertex2f(cx - 9.4f, hip_y - 1.5f);
    glEnd();
    glColor3f(0.03f,0.03f,0.04f);
    glLineWidth(1.2f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(cx - 12.5f, sh_y - 1.0f);
    glVertex2f(cx + 12.5f, sh_y - 1.0f);
    glVertex2f(cx + 9.4f, hip_y - 1.5f);
    glVertex2f(cx - 9.4f, hip_y - 1.5f);
    glEnd();


    glcol(overalls);
    fill_rect(cx - 7.8f, hip_y - 1.5f, 15.6f, 19.5f);
    draw_limb(cx - 7.2f, sh_y - 1.0f, cx - 3.1f, hip_y + 16.5f, 1.4f, overalls);
    draw_limb(cx + 7.2f, sh_y - 1.0f, cx + 3.1f, hip_y + 16.5f, 1.4f, overalls);
    glColor3f(0.03f,0.03f,0.04f);
    stroke_rect(cx - 7.8f, hip_y - 1.5f, 15.6f, 19.5f);
    glcol(trim);
    fill_circle(cx - 4.4f, hip_y + 14.0f, 1.5f, 8);
    fill_circle(cx + 4.4f, hip_y + 14.0f, 1.5f, 8);


    if(carrying){
        draw_limb(cx - 11.0f, sh_y - 1.0f, cx - 15.2f, sh_y + 13.0f, 3.0f, skin);
        draw_limb(cx + 11.0f, sh_y - 1.0f, cx + 15.2f, sh_y + 13.0f, 3.0f, skin);
        fill_circle(cx - 15.2f, sh_y + 15.0f, 3.5f, 12);
        fill_circle(cx + 15.2f, sh_y + 15.0f, 3.5f, 12);
    } else {
        draw_limb(cx - 11.3f, sh_y - 1.0f, cx - 16.3f, hip_y + 10.0f, 3.0f, skin);
        draw_limb(cx + 11.3f, sh_y - 1.0f, cx + 16.3f, hip_y + 10.0f, 3.0f, skin);
        fill_circle(cx - 16.3f, hip_y + 9.0f, 3.3f, 12);
        fill_circle(cx + 16.3f, hip_y + 9.0f, 3.3f, 12);
    }


    glcol(skin);
    fill_rect(cx - 3.5f, sh_y - 0.5f, 7.0f, NECK + 3.0f);


    glcol(skin);
    fill_circle(cx, head_y, HR, 24);
    glColor3f(0.08f,0.06f,0.05f);
    glLineWidth(1.1f);
    stroke_circle(cx, head_y, HR, 24);
    glcol(hair);
    fill_ellipse(cx, head_y + 6.0f, HR * 0.86f, HR * 0.45f, 18);
    fill_circle(cx - 6.5f, head_y + 3.0f, 3.2f, 10);
    fill_circle(cx + 6.2f, head_y + 3.8f, 2.8f, 10);


    glcol(trim);
    fill_ellipse(cx, head_y + 7.8f, 10.0f, 4.5f, 18);
    fill_rect(cx - 11.0f, head_y + 5.8f, 22.0f, 3.4f);
    glColor3f(0.05f,0.05f,0.06f);
    glLineWidth(1.0f);
    draw_line(cx, head_y + 6.1f, cx, head_y + 12.5f);
    draw_line(cx - 6.0f, head_y + 6.4f, cx - 6.0f, head_y + 10.2f);
    draw_line(cx + 6.0f, head_y + 6.4f, cx + 6.0f, head_y + 10.2f);


    glColor3f(1,1,1);
    fill_circle(cx - 3.4f, head_y + 1.4f, 2.0f, 10);
    fill_circle(cx + 3.4f, head_y + 1.4f, 2.0f, 10);
    glColor3f(0.1f,0.1f,0.1f);
    fill_circle(cx - 3.4f, head_y + 1.4f, 0.9f, 8);
    fill_circle(cx + 3.4f, head_y + 1.4f, 0.9f, 8);


    glColor3f(0,0,0);
    glLineWidth(1.2f);
    glBegin(GL_LINE_STRIP);
    for(int i = 0; i <= 8; i++){
        float a = PI + (float)i/8.0f * PI;
        glVertex2f(cx + cosf(a)*3.8f, head_y - 2.0f + sinf(a)*2.1f);
    }
    glEnd();


    glcol(trim);
    fill_rect(cx + 3.0f, hip_y + 5.0f, 3.8f, 3.0f);


    glColor3f(0.58f, 0.58f, 0.70f);
    char lbl[8];
    if     (role == 0) snprintf(lbl, sizeof(lbl), "SRC");
    else if(role == 2) snprintf(lbl, sizeof(lbl), "SINK");
    else               snprintf(lbl, sizeof(lbl), "M%d", role);
    bstr(cx - 9, fy - 16, lbl, GLUT_BITMAP_HELVETICA_10);

    glLineWidth(1.0f);
}




static void draw_status_arrow(float cx, float cy, piece_status st){
    if(st != MOVING_FORWARD && st != MOVING_BACKWARD)
        return;

    float dir = (st == MOVING_FORWARD) ? 1.0f : -1.0f;
    Col c = status_col(st);

    glcol4(c, 0.85f);
    glLineWidth(2.2f);
    draw_line(cx - dir * 21.0f, cy + 28.0f, cx + dir * 21.0f, cy + 28.0f);
    glBegin(GL_TRIANGLES);
    glVertex2f(cx + dir * 24.0f, cy + 28.0f);
    glVertex2f(cx + dir * 14.0f, cy + 34.0f);
    glVertex2f(cx + dir * 14.0f, cy + 22.0f);
    glEnd();
    glLineWidth(1.0f);
}

static void furniture_outline_rect(float x, float y, float w, float h){
    glColor3f(0.03f, 0.03f, 0.04f);
    glLineWidth(1.5f);
    stroke_rect(x, y, w, h);
}

static void draw_chair(float cx, float cy, Col c){
    glcol(c);
    fill_rect(cx - 13, cy + 5, 24, 8);
    fill_rect(cx - 13, cy + 13, 7, 18);
    fill_rect(cx + 8, cy + 13, 4, 17);
    fill_rect(cx - 11, cy - 8, 4, 13);
    fill_rect(cx + 6, cy - 8, 4, 13);
    furniture_outline_rect(cx - 13, cy + 5, 24, 8);
    furniture_outline_rect(cx - 13, cy + 13, 7, 18);
    furniture_outline_rect(cx + 8, cy + 13, 4, 17);
}

static void draw_table(float cx, float cy, Col c){
    glcol(c);
    fill_rect(cx - 18, cy + 15, 36, 7);
    fill_rect(cx - 14, cy - 7, 5, 22);
    fill_rect(cx + 9, cy - 7, 5, 22);
    fill_rect(cx - 16, cy + 22, 32, 4);
    furniture_outline_rect(cx - 18, cy + 15, 36, 7);
    furniture_outline_rect(cx - 14, cy - 7, 5, 22);
    furniture_outline_rect(cx + 9, cy - 7, 5, 22);
}

static void draw_sofa(float cx, float cy, Col c){
    glcol(c);
    fill_rect(cx - 20, cy + 2, 40, 16);
    fill_rect(cx - 23, cy + 10, 8, 14);
    fill_rect(cx + 15, cy + 10, 8, 14);
    fill_rect(cx - 17, cy + 18, 34, 12);
    fill_rect(cx - 15, cy - 4, 4, 6);
    fill_rect(cx + 11, cy - 4, 4, 6);
    furniture_outline_rect(cx - 20, cy + 2, 40, 16);
    furniture_outline_rect(cx - 23, cy + 10, 8, 14);
    furniture_outline_rect(cx + 15, cy + 10, 8, 14);
    furniture_outline_rect(cx - 17, cy + 18, 34, 12);
}

static void draw_lamp(float cx, float cy, Col c){
    glcol(c);
    fill_rect(cx - 3, cy - 2, 6, 24);
    fill_rect(cx - 11, cy - 6, 22, 5);
    glBegin(GL_QUADS);
    glVertex2f(cx - 15, cy + 20);
    glVertex2f(cx + 15, cy + 20);
    glVertex2f(cx + 9,  cy + 34);
    glVertex2f(cx - 9,  cy + 34);
    glEnd();
    furniture_outline_rect(cx - 3, cy - 2, 6, 24);
    furniture_outline_rect(cx - 11, cy - 6, 22, 5);
    glColor3f(0.03f, 0.03f, 0.04f);
    glLineWidth(1.5f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(cx - 15, cy + 20);
    glVertex2f(cx + 15, cy + 20);
    glVertex2f(cx + 9,  cy + 34);
    glVertex2f(cx - 9,  cy + 34);
    glEnd();
}

static void draw_cabinet(float cx, float cy, Col c){
    glcol(c);
    fill_rect(cx - 17, cy - 4, 34, 36);
    glColor3f(0.03f, 0.03f, 0.04f);
    glLineWidth(1.5f);
    stroke_rect(cx - 17, cy - 4, 34, 36);
    draw_line(cx, cy - 4, cx, cy + 32);
    draw_line(cx - 17, cy + 14, cx + 17, cy + 14);
    fill_circle(cx - 5, cy + 17, 1.8f, 8);
    fill_circle(cx + 5, cy + 17, 1.8f, 8);
}

static void draw_bed(float cx, float cy, Col c){
    glcol(c);
    fill_rect(cx - 22, cy + 4, 44, 17);
    fill_rect(cx - 22, cy + 21, 8, 13);
    fill_rect(cx - 15, cy + 14, 12, 7);
    fill_rect(cx - 20, cy - 3, 4, 7);
    fill_rect(cx + 15, cy - 3, 4, 7);
    furniture_outline_rect(cx - 22, cy + 4, 44, 17);
    furniture_outline_rect(cx - 22, cy + 21, 8, 13);
    furniture_outline_rect(cx - 15, cy + 14, 12, 7);
}

static void draw_furniture(float cx, float cy, piece_status st, int serial, int order){
    Col c = status_col(st);
    int kind = abs(order + serial) % 6;

    draw_status_arrow(cx, cy, st);

    if(st == MOVING_FORWARD || st == MOVING_BACKWARD){
        glcol4(c, 0.22f);
        fill_rect(cx - 27, cy - 12, 54, 54);
    }

    switch(kind){
        case 0: draw_chair(cx, cy, c);   break;
        case 1: draw_table(cx, cy, c);   break;
        case 2: draw_sofa(cx, cy, c);    break;
        case 3: draw_lamp(cx, cy, c);    break;
        case 4: draw_cabinet(cx, cy, c); break;
        default: draw_bed(cx, cy, c);    break;
    }

    if(st == MOVING_BACKWARD || st == BLOCKED){
        glColor3f(0.95f, 0.10f, 0.10f);
        glLineWidth(2.0f);
        draw_line(cx - 24, cy - 10, cx + 24, cy + 38);
        draw_line(cx - 24, cy + 38, cx + 24, cy - 10);
    }

    glColor3f(1,1,1);
    char lbl[10];
    snprintf(lbl, sizeof(lbl), "#%d", serial);
    bstr(cx - 12, cy - 16, lbl, GLUT_BITMAP_HELVETICA_10);

    glLineWidth(1.0f);
}








static void draw_building_shadow(float cx, float fy, float w){
    glColor4f(0,0,0,0.16f);
    fill_ellipse(cx, fy - 1.0f, w * 0.52f, 5.0f, 18);
}

static void draw_truck_body(float cx, float fy, Col accent){
    float base_y = fy + 2.0f;
    Col body = {0.86f, 0.88f, 0.86f};
    Col glass = {0.34f, 0.57f, 0.76f};
    Col tire = {0.04f, 0.04f, 0.05f};
    Col rim = {0.72f, 0.74f, 0.74f};

    draw_building_shadow(cx, fy, 96.0f);

    glcol(body);
    fill_rect(cx - 46.0f, base_y + 16.0f, 55.0f, 42.0f);
    fill_rect(cx + 9.0f, base_y + 21.0f, 31.0f, 34.0f);
    glColor3f(0.05f, 0.05f, 0.06f);
    glLineWidth(1.2f);
    stroke_rect(cx - 46.0f, base_y + 16.0f, 55.0f, 42.0f);
    stroke_rect(cx + 9.0f, base_y + 21.0f, 31.0f, 34.0f);

    glcol(accent);
    fill_rect(cx - 42.0f, base_y + 50.0f, 46.0f, 7.0f);
    fill_rect(cx - 40.0f, base_y + 25.0f, 30.0f, 14.0f);

    glcol(glass);
    glBegin(GL_QUADS);
    glVertex2f(cx + 15.0f, base_y + 42.0f);
    glVertex2f(cx + 33.0f, base_y + 42.0f);
    glVertex2f(cx + 29.0f, base_y + 53.0f);
    glVertex2f(cx + 15.0f, base_y + 53.0f);
    glEnd();
    glColor3f(0.05f, 0.05f, 0.06f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(cx + 15.0f, base_y + 42.0f);
    glVertex2f(cx + 33.0f, base_y + 42.0f);
    glVertex2f(cx + 29.0f, base_y + 53.0f);
    glVertex2f(cx + 15.0f, base_y + 53.0f);
    glEnd();

    glcol(tire);
    fill_circle(cx - 28.0f, base_y + 13.0f, 9.0f, 18);
    fill_circle(cx + 24.0f, base_y + 13.0f, 9.0f, 18);
    glcol(rim);
    fill_circle(cx - 28.0f, base_y + 13.0f, 4.0f, 14);
    fill_circle(cx + 24.0f, base_y + 13.0f, 4.0f, 14);

    glColor3f(1.0f, 0.92f, 0.38f);
    fill_rect(cx + 38.0f, base_y + 30.0f, 4.0f, 7.0f);
    glColor3f(0.95f, 0.12f, 0.10f);
    fill_rect(cx - 48.0f, base_y + 30.0f, 4.0f, 8.0f);

    glColor3f(0.05f, 0.05f, 0.06f);
    bstr(cx - 33.0f, base_y + 31.0f, "DELIVERY", GLUT_BITMAP_HELVETICA_10);

    glLineWidth(1.0f);
}

static void draw_truck(float cx, float fy, Col accent){
    glPushMatrix();
    glTranslatef(cx, fy, 0.0f);
    glScalef(1.18f, 1.18f, 1.0f);
    draw_truck_body(0.0f, 0.0f, accent);
    glPopMatrix();
}

static void draw_home_body(float cx, float fy, Col accent){
    float base_y = fy + 2.0f;
    float w = 78.0f;
    float h = 78.0f;
    Col wall = {0.78f, 0.72f, 0.62f};
    Col roof = {0.44f, 0.16f, 0.12f};
    Col glass = {0.32f, 0.55f, 0.74f};

    draw_building_shadow(cx, fy, w);

    glcol(wall);
    fill_rect(cx - w/2.0f, base_y, w, h);
    glColor3f(0.05f, 0.05f, 0.06f);
    glLineWidth(1.2f);
    stroke_rect(cx - w/2.0f, base_y, w, h);

    glcol(roof);
    glBegin(GL_TRIANGLES);
    glVertex2f(cx - w/2.0f - 8.0f, base_y + h);
    glVertex2f(cx + w/2.0f + 8.0f, base_y + h);
    glVertex2f(cx, base_y + h + 46.0f);
    glEnd();
    glColor3f(0.05f, 0.05f, 0.06f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(cx - w/2.0f - 8.0f, base_y + h);
    glVertex2f(cx + w/2.0f + 8.0f, base_y + h);
    glVertex2f(cx, base_y + h + 46.0f);
    glEnd();

    glcol(accent);
    fill_rect(cx - 10.0f, base_y, 20.0f, 50.0f);
    glColor3f(0.05f, 0.05f, 0.06f);
    stroke_rect(cx - 10.0f, base_y, 20.0f, 50.0f);
    fill_circle(cx + 5.0f, base_y + 25.0f, 1.6f, 8);

    glcol(glass);
    fill_rect(cx - 31.0f, base_y + 37.0f, 17.0f, 25.0f);
    fill_rect(cx + 15.0f, base_y + 37.0f, 17.0f, 25.0f);
    glColor3f(0.05f, 0.05f, 0.06f);
    stroke_rect(cx - 31.0f, base_y + 37.0f, 17.0f, 25.0f);
    stroke_rect(cx + 15.0f, base_y + 37.0f, 17.0f, 25.0f);
    draw_line(cx - 22.5f, base_y + 37.0f, cx - 22.5f, base_y + 62.0f);
    draw_line(cx + 23.5f, base_y + 37.0f, cx + 23.5f, base_y + 62.0f);

    glColor3f(0.06f, 0.06f, 0.07f);
    bstr(cx - 14.0f, base_y - 12.0f, "HOME", GLUT_BITMAP_HELVETICA_10);

    glLineWidth(1.0f);
}

static void draw_home(float cx, float fy, Col accent){
    glPushMatrix();
    glTranslatef(cx, fy, 0.0f);
    glScalef(1.16f, 1.16f, 1.0f);
    draw_home_body(0.0f, 0.0f, accent);
    glPopMatrix();
}

static float compact_slot_x(int slot, float lx, float lw){
    return lx + (lw / 6.0f) * (float)(slot + 1);
}

static float lerp(float a, float b, float t){
    return a + (b - a) * t;
}


static float node_x(int m, float lx, float lw, int n){
    if(n > 5){
        int mid = n / 2;

        if(m <= 0)
            return compact_slot_x(0, lx, lw);
        if(m == 1)
            return compact_slot_x(1, lx, lw);
        if(m < mid){
            float denom = (float)(mid - 1);
            float t = (denom > 0.0f) ? (float)(m - 1) / denom : 0.0f;
            return lerp(compact_slot_x(1, lx, lw), compact_slot_x(2, lx, lw), t);
        }
        if(m == mid)
            return compact_slot_x(2, lx, lw);
        if(m < n - 2){
            float denom = (float)(n - 2 - mid);
            float t = (denom > 0.0f) ? (float)(m - mid) / denom : 0.0f;
            return lerp(compact_slot_x(2, lx, lw), compact_slot_x(3, lx, lw), t);
        }
        if(m == n - 2)
            return compact_slot_x(3, lx, lw);
        return compact_slot_x(4, lx, lw);
    }

    return lx + (lw / (float)(n + 1)) * (float)(m + 1);
}


static float held_y(float fy){

    return fy + 24.0f + 26.0f + 15.0f;
}

static int compact_workers(void){
    return g_plen > 5;
}

static int compact_middle_worker(int n){
    return n / 2;
}

static int worker_is_visible(int m, int n){
    if(n <= 5)
        return 1;
    return m == 0 || m == 1 || m == compact_middle_worker(n) ||
           m == n - 2 || m == n - 1;
}

static void draw_dot_group(float x0, float x1, float y){
    float cx = (x0 + x1) * 0.5f;
    glColor4f(0.88f, 0.88f, 0.95f, 0.72f);
    fill_circle(cx - 10.0f, y, 3.0f, 12);
    fill_circle(cx,         y, 3.0f, 12);
    fill_circle(cx + 10.0f, y, 3.0f, 12);
}

static void draw_worker_gaps(float lx, float lw, float fy, int n){
    if(!compact_workers())
        return;

    int mid = compact_middle_worker(n);
    float y = fy + 49.0f;

    if(mid > 2)
        draw_dot_group(node_x(1, lx, lw, n), node_x(mid, lx, lw, n), y);
    if(n - 2 > mid + 1)
        draw_dot_group(node_x(mid, lx, lw, n), node_x(n - 2, lx, lw, n), y);
}

static float package_lane_offset(piece_status st, int serial){
    float jitter = (float)((serial % 3) - 1) * 2.5f;
    if(st == MOVING_FORWARD)
        return 11.0f + jitter;
    if(st == MOVING_BACKWARD)
        return -11.0f + jitter;
    return jitter;
}

static int pile_cols(void){
    if(g_fc <= 4) return g_fc > 0 ? g_fc : 1;
    if(g_fc <= 12) return 4;
    if(g_fc <= 30) return 5;
    if(g_fc <= 120) return 8;
    return 12;
}

static float pile_dx(void){
    if(g_fc > 120) return 7.0f;
    if(g_fc > 40) return 10.0f;
    return (g_fc > 20) ? 14.0f : 18.0f;
}

static float pile_dy(void){
    if(g_fc > 120) return 5.0f;
    if(g_fc > 40) return 7.0f;
    return (g_fc > 20) ? 10.0f : 13.0f;
}

static float pile_piece_scale(void){
    if(g_fc > 120) return 0.32f;
    if(g_fc > 40) return 0.48f;
    if(g_fc > 20) return 0.72f;
    if(g_fc > 10) return 0.86f;
    return 1.0f;
}

static void draw_furniture_scaled(float cx, float cy, piece_status st, int serial, int order, float scale){
    if(scale >= 0.99f){
        draw_furniture(cx, cy, st, serial, order);
        return;
    }

    glPushMatrix();
    glTranslatef(cx, cy, 0.0f);
    glScalef(scale, scale, 1.0f);
    draw_furniture(0.0f, 0.0f, st, serial, order);
    glPopMatrix();
}

static void draw_street(float lx, float lw, float fy){
    float road_y = fy - 14.0f;
    float road_h = 105.0f;

    glColor3f(0.11f, 0.12f, 0.14f);
    fill_rect(lx - 24.0f, road_y, lw + 48.0f, road_h);

    glColor3f(0.18f, 0.19f, 0.20f);
    fill_rect(lx - 24.0f, road_y + road_h - 10.0f, lw + 48.0f, 10.0f);
    fill_rect(lx - 24.0f, road_y, lw + 48.0f, 8.0f);

    glColor4f(1.0f, 0.88f, 0.25f, 0.75f);
    for(float x = lx - 10.0f; x < lx + lw + 10.0f; x += 56.0f){
        fill_rect(x, fy + 22.0f, 30.0f, 3.0f);
    }

    glColor4f(1.0f, 1.0f, 1.0f, 0.22f);
    for(float x = lx - 18.0f; x < lx + lw + 18.0f; x += 36.0f){
        fill_rect(x, road_y + road_h - 16.0f, 16.0f, 2.0f);
    }
}

static void draw_lane(int team, float lane_cx, float fy, float lw,
                      Col team_col, int team_id){
    int   n  = g_plen;
    float lx = lane_cx - lw / 2.0f;

    draw_street(lx, lw, fy);


    float pipe_y = fy + 28.0f;
    glcol(C_PIPE);
    fill_rect(lx + 8, pipe_y - 7, lw - 16, 14);

    glColor4f(1,1,1, 0.07f);
    fill_rect(lx + 8, pipe_y + 2, lw - 16, 4);

    glcol(C_PIPE);
    fill_circle(lx + 8,       pipe_y, 7, 14);
    fill_circle(lx + lw - 8,  pipe_y, 7, 14);


    float mid = lx + lw / 2.0f;
    glColor4f(1,1,1, 0.18f);
    glLineWidth(1.5f);
    draw_line(mid - 12, pipe_y, mid + 12, pipe_y);
    glBegin(GL_TRIANGLES);
    glVertex2f(mid + 12, pipe_y);
    glVertex2f(mid + 5,  pipe_y + 5);
    glVertex2f(mid + 5,  pipe_y - 5);
    glEnd();


    glcol(C_FLOOR);
    glLineWidth(2.0f);
    draw_line(lx, fy, lx + lw, fy);
    draw_worker_gaps(lx, lw, fy, n);


    char tl[16];
    snprintf(tl, sizeof(tl), "TEAM %d", team_id);
    Col bright = { fminf(team_col.r*1.4f, 1.0f),
                   fminf(team_col.g*1.4f, 1.0f),
                   fminf(team_col.b*1.4f, 1.0f) };
    glcol(bright);
    bstr(lx - 74, fy + 16, tl, GLUT_BITMAP_HELVETICA_18);
    if(compact_workers()){
        char visible_msg[48];
        snprintf(visible_msg, sizeof(visible_msg), "showing 5 of %d workers", n);
        glColor3f(0.78f, 0.78f, 0.86f);
        bstr(lx - 74, fy + 1, visible_msg, GLUT_BITMAP_HELVETICA_10);
    }

    float first_x = node_x(0, lx, lw, n);
    float last_x = node_x(n - 1, lx, lw, n);
    float truck_x = fmaxf(lx + 52.0f, first_x - 88.0f);
    float home_x = fminf(lx + lw - 46.0f, last_x + 88.0f);
    draw_truck(truck_x, fy, bright);
    draw_home(home_x, fy, bright);


    AnimPiece *ap = g_anim[team];


    int holding[MAX_PIECES];
    memset(holding, 0, sizeof(holding));

    for(int p = 0; p < g_fc && p < MAX_PIECES; p++){
        AnimPiece *a = &ap[p];
        if(!a->active || a->traveling) continue;


        int at_source = (a->cur_node == 0);
        int at_sink   = (a->cur_node == n - 1);
        if(!at_source && !at_sink && a->cur_node >= 0 && a->cur_node < n &&
           a->cur_node < MAX_PIECES && worker_is_visible(a->cur_node, n))
            holding[a->cur_node] = 1;
    }


    for(int m = 0; m < n; m++){
        if(!worker_is_visible(m, n))
            continue;
        float wx   = node_x(m, lx, lw, n);
        int   role = (m == 0) ? 0 : (m == n-1) ? 2 : 1;
        draw_worker(wx, fy, team_col, role, (m < MAX_PIECES) ? holding[m] : 0);
    }


    float hy      = held_y(fy);
    int   src_i   = 0;
    int   snk_i   = 0;
    int   cols    = pile_cols();
    float pdx     = pile_dx();
    float pdy     = pile_dy();
    float pscale  = pile_piece_scale();
    float source_side_x = node_x(0, lx, lw, n) - 56.0f;
    float sink_side_x   = node_x(n - 1, lx, lw, n) + 56.0f;

    for(int p = 0; p < g_fc && p < MAX_PIECES; p++){
        AnimPiece *a = &ap[p];
        if(!a->active) continue;

        piece_status st = a->status;


        if(!a->traveling && a->cur_node == 0){
            int col = src_i % cols;
            int row = src_i / cols;
            float px = source_side_x + (float)col * pdx;
            float py = fy + 8.0f + (float)row * pdy;
            piece_status pile_status = (st == MOVING_BACKWARD) ? BLOCKED : st;
            draw_furniture_scaled(px, py, pile_status, a->serial_no, a->order, pscale);
            src_i++;
            continue;
        }


        if(!a->traveling && a->cur_node == n-1){
            int col = snk_i % cols;
            int row = snk_i / cols;
            float px = sink_side_x + (float)col * pdx;
            float py = fy + 8.0f + (float)row * pdy;
            draw_furniture_scaled(px, py, PLACED, a->serial_no, a->order, pscale);
            snk_i++;
            continue;
        }


        float px, py;
        if(!a->traveling){

            px = node_x(a->cur_node, lx, lw, n);
            py = hy + package_lane_offset(st, a->serial_no) +
                 3.5f * sinf(a->bob_phase);
        } else {

            float t  = ease(a->progress);
            float x0 = node_x(a->prev_node, lx, lw, n);
            float x1 = node_x(a->dest_node, lx, lw, n);
            px = x0 + (x1 - x0) * t;
            py = hy + package_lane_offset(st, a->serial_no) +
                 20.0f * sinf(a->progress * PI);
        }
        draw_furniture(px, py, st, a->serial_no, a->order);
    }
}




static void draw_header(int round, int t1w, int t2w, int wr, int champ){
    glcol(C_HDR);
    fill_rect(0, (float)g_H - 64, (float)g_W, 64);

    glColor3f(1.0f, 0.85f, 0.22f);
    bstr(14, (float)g_H - 26,
         "HOME FURNISHING COMPETITION", GLUT_BITMAP_HELVETICA_18);

    char buf[64];
    snprintf(buf, sizeof(buf), "Round %d", round);
    glColor3f(0.9f, 0.9f, 0.9f);
    bstr((float)g_W/2 - 28, (float)g_H - 26, buf, GLUT_BITMAP_HELVETICA_18);

    snprintf(buf, sizeof(buf), "T1: %d   T2: %d   (first to %d)", t1w, t2w, wr);
    glColor3f(0.65f, 0.65f, 0.75f);
    bstr((float)g_W - 272, (float)g_H - 26, buf, GLUT_BITMAP_HELVETICA_12);

    snprintf(buf, sizeof(buf), "Workers: %d   Pieces: %d", g_plen, g_fc);
    glColor3f(0.58f, 0.58f, 0.68f);
    bstr((float)g_W - 272, (float)g_H - 43, buf, GLUT_BITMAP_HELVETICA_10);


    for(int t = 0; t < 2; t++){
        int   wins = (t == 0) ? t1w : t2w;
        float bx   = (t == 0) ? 14 : (float)g_W/2 + 16;
        float by   = (float)g_H - 50;
        for(int w = 0; w < wr; w++){
            glColor3f(w < wins ? 0.20f : 0.22f,
                      w < wins ? 0.82f : 0.22f,
                      w < wins ? 0.32f : 0.30f);
            fill_rect(bx + (float)w*22, by, 18, 10);
            glColor3f(0,0,0);
            stroke_rect(bx + (float)w*22, by, 18, 10);
        }
    }


    if(champ > 0){
        float pulse = 0.55f + 0.45f * sinf(g_clock * 3.5f);
        float bx = (float)g_W/2 - 210, by = (float)g_H/2 - 55;
        glColor4f(0.92f, 0.72f, 0.0f, pulse * 0.95f);
        fill_rect(bx, by, 420, 90);
        glColor3f(0.05f, 0.05f, 0.05f);
        snprintf(buf, sizeof(buf), "  WINNER: TEAM %d!  ", champ);
        bstr(bx + 50, by + 30, buf, GLUT_BITMAP_TIMES_ROMAN_24);
        glColor3f(0.2f, 0.2f, 0.2f);
        bstr(bx + 90, by + 10, "Press ESC to close", GLUT_BITMAP_HELVETICA_12);
    }
}




static void draw_legend(float x, float y){
    struct { piece_status s; const char *name; } rows[] = {
        { AVAILABLE,       "Waiting at source"     },
        { MOVING_FORWARD,  "Moving forward"        },
        { MOVING_BACKWARD, "Rejected – going back" },
        { PLACED,          "Placed at sink"        },
        { BLOCKED,         "Blocked"               },
    };
    glColor3f(0.55f, 0.55f, 0.66f);
    bstr(x, y + 118, "LEGEND", GLUT_BITMAP_HELVETICA_12);
    for(int i = 0; i < 5; i++){
        float ry = y + (float)(4 - i) * 23.0f;
        glcol(status_col(rows[i].s));
        fill_rect(x, ry, 13, 13);
        glColor3f(0,0,0);
        stroke_rect(x, ry, 13, 13);
        glColor3f(0.85f, 0.85f, 0.90f);
        bstr(x + 17, ry + 1, rows[i].name, GLUT_BITMAP_HELVETICA_10);
    }
}

static Col confetti_col(int i){
    switch(i % 6){
        case 0: return (Col){1.00f, 0.30f, 0.24f};
        case 1: return (Col){1.00f, 0.84f, 0.18f};
        case 2: return (Col){0.22f, 0.82f, 0.34f};
        case 3: return (Col){0.24f, 0.64f, 1.00f};
        case 4: return (Col){0.76f, 0.42f, 1.00f};
        default: return (Col){1.00f, 0.48f, 0.78f};
    }
}

static void draw_confetti(void){
    int count = 150;
    for(int i = 0; i < count; i++){
        float seed = (float)((i * 73) % 997) / 997.0f;
        float x = seed * (float)g_W;
        float speed = 38.0f + (float)(i % 9) * 11.0f;
        float y = fmodf((float)g_H + (float)((i * 41) % 260) - g_clock * speed,
                        (float)g_H + 120.0f) - 60.0f;
        float drift = sinf(g_clock * (0.8f + (float)(i % 5) * 0.17f) + (float)i) * 22.0f;
        float rot = g_clock * (1.6f + (float)(i % 7) * 0.23f) + (float)i;
        float w = 5.0f + (float)(i % 4);
        float h = 9.0f + (float)(i % 3) * 2.0f;
        Col c = confetti_col(i);

        glPushMatrix();
        glTranslatef(x + drift, y, 0.0f);
        glRotatef(rot * 57.29578f, 0.0f, 0.0f, 1.0f);
        glcol4(c, 0.92f);
        fill_rect(-w * 0.5f, -h * 0.5f, w, h);
        glPopMatrix();
    }
}

static float visual_margin(void){
    float margin = (g_plen <= 5) ? 130.0f : 96.0f;
    if(g_fc > 20)
        margin += 18.0f;
    if((float)g_W - margin * 2.0f < 560.0f)
        margin = fmaxf(62.0f, ((float)g_W - 560.0f) * 0.5f);
    return margin;
}


