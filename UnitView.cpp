//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop
#include <gl\gl.h>
#include <gl\glu.h>
#include <ctype.h>
#include "Classes.h"
#include "MathFunctions.h"
#include "Definitions.h"
#include "UnitView.h"
#include <algorithm>

//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma link "ExtCtrls"
#pragma resource "*.dfm"
TFormView *FormView;

//---------------------------------------------------------------------------
__fastcall TFormView::TFormView(TComponent* Owner) : TForm(Owner) {
    int i,j;
    Application->OnIdle=IdleLoop;
    FormView->Top=0;//ToolBarHeight;
    FormView->Left=0;
    FormView->Height=FormView->ClientHeight;
    FormView->Width=FormView->ClientWidth;

//default colors
    Red.R      =1;
    Red.G      =0.65;
    Red.B      =0.65;;
    Green.R    =0.65;
    Green.G    =1;
    Green.B    =0.65;
    Blue.R     =0.65;
    Blue.G     =0.65;
    Blue.B     =1;
    Yellow.R   =1;
    Yellow.G   =1;
    Yellow.B   =0.4;
    LightGray.R=0.55;
    LightGray.G=0.55;
    LightGray.B=0.55;
    DarkGray.R =0.4;
    DarkGray.G =0.4;
    DarkGray.B =0.4;
//default values
    a=19*PI/60;
    b=13*PI/60;
    d=10;
    f=0.5;      //approximately 30 degrees (or PI*30/180 rad)
    g=0.2;
    e=40;

    c=0;
    w=ClientWidth;
    h=ClientHeight;
    cuartoCargado=false;
    simularPropagacion=false;
    verRecorrido=false;
    verTriangulos=false;
}

//---------------------------------------------------------------------------
void __fastcall TFormView::FormCreate(TObject *Sender) {
//OpenGL initialize
    hdc=GetDC(Handle);
    SetPixelFormatDescriptor();
    hrc=wglCreateContext(hdc);
    if(hrc==NULL)
        ShowMessage("hrc==NULL");
    if(wglMakeCurrent(hdc,hrc)==false)
        ShowMessage("Could not MakeCurrent");
//OpenGL default parameters
    glClearColor(0.6,0.6,0.6,0.0);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0,1.0);
    glLineWidth(1);
    glPointSize(4);

    a   =19*PI/60;
    b   =13*PI/60;
    d   =10;
    f   =0.5;      //approximately 30 degrees (or PI*30/180 rad)
    g   =0.2;
    e   =40;
    c   =0;
}

//---------------------------------------------------------------------------
void __fastcall TFormView::FormResize(TObject *Sender) {
    //resize OpenGL viewport
    w=ClientWidth;
    h=ClientHeight;
    if(h==0)h=1;
    glViewport(0,0,w,h);
}

//---------------------------------------------------------------------------
void __fastcall TFormView::SetPixelFormatDescriptor() {
//OpenGL initialize
    PIXELFORMATDESCRIPTOR pfd= {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,
        PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,
        24,
        0,0,0,0,0,0,
        0,0,
        0,0,0,0,0,
        32,
        0,
        0,
        PFD_MAIN_PLANE,
        0,
        0,0,
    };
    PixelFormat=ChoosePixelFormat(hdc,&pfd);
    SetPixelFormat(hdc,PixelFormat,&pfd);
}

//---------------------------------------------------------------------------
inline void __fastcall TFormView::glVertexp(point p)
{
//draw point
    glVertex3f(p.x,p.y,p.z);
}

//---------------------------------------------------------------------------
inline void __fastcall TFormView::glColorc(color c)
{
//color RGB
    glColor3f(c.R,c.G,c.B);
}

//---------------------------------------------------------------------------
void __fastcall TFormView::IdleLoop(TObject*,bool& done) {
//draw scene
    DrawScene();
}

//---------------------------------------------------------------------------
void __fastcall TFormView::DrawScene(void)
{
    // ----- Ajuste de la proyecci�n y c�mara -----
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    double df, db;
    db = sqrt(3*e*g*e*g + 2*e*g*(fabs(c.x)+fabs(c.y)+fabs(c.z)) + c.x*c.x + c.y*c.y + c.z*c.z);
    if (d > db)
        df = db;
    else
        df = 0.99*d;

    gluPerspective(
        180*f/PI,         // FOV vertical (grados)
        double(w)/double(h), // aspect ratio
        d - df,           // near plane
        d + db            // far plane
    );

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // C�mara centrada en 'o' mirando hacia 'c' con 'Up' como up vector
    o.x = d*cos(b)*cos(a)+c.x;
    o.y = d*cos(b)*sin(a)+c.y;
    o.z = d*sin(b)+c.z;
    Up.x = -sin(a);
    Up.y =  cos(a);
    Up.z =  0;
    Up = Normal(Up/(c-o));

    gluLookAt(o.x,o.y,o.z,c.x,c.y,c.z,Up.x,Up.y,Up.z);

    // Limpieza de buffers
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // ----- Dibujo de rayos (si estamos simulando y hay datos 'ry') -----
    if (simularPropagacion && ry)
    {
        // Recorremos cada rayo
        for (int i = 0; i < s.NRAYS; i++)
        {
            // Si el rayo est� perdido, igual podr�amos dibujar
            // hasta el �ltimo punto, o simplemente omitirlo:
            if (ry[i].lost == false)
            {
                double distAcum = 0.0;

                // Cada rayo tiene ry[i].N puntos de reflexi�n
                // Ej: r[0] = origen, r[1] = primer choque, etc.
                for (int j = 0; j < ry[i].N - 1; j++)
                {
                    point pInicio    = ry[i].r[j];
                    point pSiguiente = ry[i].r[j + 1];
                    // La distancia entre r[j] y r[j+1] suele estar en d[j+1]
                    // (depende de c�mo guardaste los �ndices)
                    double distSegmento = ry[i].d[j + 1];

                    double distIniSegmento = distAcum;
                    double distFinSegmento = distAcum + distSegmento;

                    // Avanzamos el acumulador para el siguiente tramo
                    distAcum = distFinSegmento;

                    // Ahora comparamos 'distancia' con el tramo [distIniSegmento, distFinSegmento]
                    if (distancia < distIniSegmento)
                    {
                        // A�n no lleg� a este tramo => no dibujamos nada
                        continue;
                    }
                    else if (distancia >= distFinSegmento)
                    {
                        // Tramo completo => lo dibujamos con f=false
                        // para que DrawVector represente la l�nea entera
                        // sin la parte "parcial"
                        vector vCompleto = pSiguiente - pInicio;
                        DrawVector(pInicio, vCompleto, false, distIniSegmento, i, j);
                    }
                    else
                    {
                        // distancia est� en medio de este tramo
                        // => dibujamos parte parcial
                        vector vCompleto = pSiguiente - pInicio;
                        DrawVector(pInicio, vCompleto, true, distIniSegmento, i, j);
                    }
                }
            }
            else
            {
                // Rayo perdido: podr�as dibujarlo hasta su �ltimo punto...
                // o no dibujarlo en absoluto.
            }
        }
    }

    // ----- Dibujar la sala (si ya se carg�) -----
    if (cuartoCargado)
    {
        // Si quieres "pintar" los planos con un mapa de calor, etc.
        int tmpG = 0;
        if (simularPropagacion) {
            tmpG = floor(1000 * distancia / V_SON);
            if (tmpG >= durSim) {
                tmpG = durSim - 1;
            }
        }
        // Recorremos los planos y los dibujamos
        for (int i = 0; i < r.NP; i++) {
            DrawPlane(r.p[i], tmpG);
        }

        // Dibujamos la fuente y el receptor
        DrawSource(s);
        DrawReceptor(r1);
    }

    // Dibujar ejes (opcional)
    glDisable(GL_DEPTH_TEST);
    DrawAxis();
    glEnable(GL_DEPTH_TEST);

    // Intercambio de buffers
    SwapBuffers(hdc);
}
//---------------------------------------------------------------------------
inline void __fastcall TFormView::DrawVector(point p, vector v, bool f, float m, int i, int j){
    glColorc(Yellow);

    if(f){//gr�fico din�mico del vector
        double mod, rad;
        v=Normal(v);
        mod=distancia-m;
        v=v*mod;
        rad=0.1*pow((1-r.p[ry[i].Plane[j]].m.alfa)*(1-r.p[ry[i].Plane[j]].m.delta),j); ;
        glPushMatrix();
              glTranslatef(v.x+p.x, v.y+p.y, v.z+p.z);
              gluSphere(gluNewQuadric(), rad, 35, 56);
        glPopMatrix();
    }
    if(verRecorrido){//Recorrido){
        glLineWidth(1);
        glBegin(GL_LINES);
            glVertex3f(p.x,p.y,p.z);
            glVertex3f(v.x+p.x, v.y+p.y, v.z+p.z);
        glEnd();
    }
}

//---------------------------------------------------------------------------
inline void __fastcall TFormView::DrawPlane(plane p, int tmpG) {

    int triID = 0;
    color col;
    int i;
    double x;

    for (i = 0; i < p.NT; i++) {
        triID = p.t[i].ID;


        if (simularPropagacion) {
            // Normalizamos el contador de impactos entre 0 y 1
            double normalizedImpacts = std::min(p.t[i].impactCounter / 50.0, 1.0);

            // Obtenemos el color del mapa de calor en funci�n de los impactos
            col.getHeatMapColor(normalizedImpacts);
        } else {
            col = p.Color;  // Usamos el color por defecto si no hay simulaci�n
        }

        // Dibujamos el tri�ngulo con el color calculado
        glColorc(col * (1 - (p.n * Normal(c - o) * 0.2)));
        glBegin(GL_TRIANGLES);
            glVertexp(p.t[i].p0);
            glVertexp(p.t[i].p1);
            glVertexp(p.t[i].p2);
        glEnd();

        // Dibujo del contorno del tri�ngulo si est� activado
        if (verTriangulos) {
            glColorc(col * 0.5);
            glLineWidth(1);
            glBegin(GL_LINE_LOOP);
                glVertexp(p.t[i].p0);
                glVertexp(p.t[i].p1);
                glVertexp(p.t[i].p2);
            glEnd();
        }
    }

    // Contorno del plano
    glColorc(p.Color * 0.5);
    glLineWidth(1);
    glBegin(GL_LINE_LOOP);
        for (i = 0; i < p.NP; i++)
            glVertexp(p.p[i]);
    glEnd();
}


//---------------------------------------------------------------------------
inline void __fastcall TFormView::DrawSource(source f) {

    int i;
    //inner
    glColorc(f.Color);
    glBegin(GL_TRIANGLES);
        for(i=0;i<20;i++){
            glVertexp(f.p + f.IcoFace[i].p0*f.VisualRadius);
            glVertexp(f.p + f.IcoFace[i].p1*f.VisualRadius);
            glVertexp(f.p + f.IcoFace[i].p2*f.VisualRadius);
        }
    glEnd();
//contour
    glColorc(f.Color*0.5);
    glLineWidth(1);
  	for(i=0;i<20;i++){
        glBegin(GL_LINE_LOOP);
            glVertexp(f.p + f.IcoFace[i].p0*f.VisualRadius);
            glVertexp(f.p + f.IcoFace[i].p1*f.VisualRadius);
            glVertexp(f.p + f.IcoFace[i].p2*f.VisualRadius);
     	glEnd();
    }
}

//---------------------------------------------------------------------------
inline void __fastcall TFormView::DrawReceptor(receptor q) {
    glColorc(q.Color);
    glBegin(GL_QUADS);
        for(int i=0;i<32;i++)
            for(int j=0;j<4;j++)
                glVertexp(q.p+(q.SphereFace[i][j]*q.VisualRadius));
    glEnd();

    glColorc(q.Color*0.5);
    //contour of sphere faces
    for(int i=0;i<32;i++){
        glBegin(GL_LINE_LOOP);
            for(int j=0;j<4;j++)
                glVertexp(q.p+(q.SphereFace[i][j]*q.VisualRadius));
  	    glEnd();
    }
}

//---------------------------------------------------------------------------
inline void __fastcall TFormView::DrawGrid(void) {
    double l;
//inner lines
    glColorc(LightGray);
    glBegin(GL_LINES);
    for(l=g; l<e*g-g/2; l+=g) {
        glVertex3f(l,-e*g,0);
        glVertex3f(l,e*g,0);
        glVertex3f(-e*g,l,0);
        glVertex3f(e*g,l,0);
    }
    for(l=-g; l>-e*g+g/2; l-=g) {
        glVertex3f(l,-e*g,0);
        glVertex3f(l,e*g,0);
        glVertex3f(-e*g,l,0);
        glVertex3f(e*g,l,0);
    }
    glEnd();
//contour lines
    glColorc(DarkGray);
    glBegin(GL_LINE_LOOP);
    glVertex3f(e*g,e*g,0);
    glVertex3f(e*g,-e*g,0);
    glVertex3f(-e*g,-e*g,0);
    glVertex3f(-e*g,e*g,0);
    glEnd();
//axis lines
    glBegin(GL_LINES);
    glVertex3f(-e*g,0,0);
    glVertex3f(e*g,0,0);
    glVertex3f(0,-e*g,0);
    glVertex3f(0,e*g,0);
    glVertex3f(0,0,-e*g);
    glVertex3f(0,0,e*g);
    glEnd();
}

//---------------------------------------------------------------------------
inline void __fastcall TFormView::DrawCoordinate(point p,color c) {
//draw coordinates
    glColorc(c);
    glBegin(GL_LINES);
    glVertex3f(p.x,0,0);
    glVertex3f(p.x,p.y,0);
    glVertex3f(0,p.y,0);
    glVertex3f(p.x,p.y,0);
    glVertex3f(p.x,p.y,0);
    glVertex3f(p.x,p.y,p.z);
    glEnd();
}

//---------------------------------------------------------------------------
inline void __fastcall TFormView::DrawAxis(void) {
//x, y and z coloured axis
    glLineWidth(2);
    glColorc(Red);
    glBegin(GL_LINES);
    glVertex3f(0,0,0);
    glVertex3f(0.1*e*g,0,0);
    glEnd();
    glColorc(Green);
    glBegin(GL_LINES);
    glVertex3f(0,0,0);
    glVertex3f(0,0.1*e*g,0);
    glEnd();
    glColorc(Blue);
    glBegin(GL_LINES);
    glVertex3f(0,0,0);
    glVertex3f(0,0,0.1*e*g);
    glEnd();
    glLineWidth(2);
}

//---------------------------------------------------------------------------
void __fastcall TFormView::Timer1Timer(TObject *Sender) {
    //En Definitions.h defino la velocidad SPEED
    tiempo++;
    distancia=SPEED*tiempo;
}

//---------------------------------------------------------------------------
void __fastcall TFormView::chbRecorridoClick(TObject *Sender)
{
    if(verRecorrido){
        verRecorrido=false;
    }
    else{
        verRecorrido=true;
    }
}

//---------------------------------------------------------------------------
void __fastcall TFormView::btnCargarCuboClick(TObject *Sender){
    if (!cuartoCargado){
        cuartoCargado=true;

        //Posici�n del micr�fono
        r1.p=-1.0;
        //Posici�n de la fuente sonora
        s.p=1.5;

        int ndivisiones=edTri->Text.ToInt();
        //Inicializaciones cubo
        r.NewPlanes(6);
        //-------------square 1 back
        r.p[0].NewPoints(4);
        r.p[0].p[0].x=-2.0f;
        r.p[0].p[0].y=2.0f;
        r.p[0].p[0].z=2.0f;
        r.p[0].p[1].x=-2.0f;
        r.p[0].p[1].y=-2.0f;
        r.p[0].p[1].z=2.0f;
        r.p[0].p[2].x=-2.0f;
        r.p[0].p[2].y=-2.0f;
        r.p[0].p[2].z=-2.0f;
        r.p[0].p[3].x=-2.0f;
        r.p[0].p[3].y=2.0f;
        r.p[0].p[3].z=-2.0f;
        r.p[0].MoreTriangle(ndivisiones);//PointGenTriangle();

        //-------------square 2 front
        r.p[1].NewPoints(4);
        r.p[1].p[0].x=2.0f;
        r.p[1].p[0].y=2.0f;
        r.p[1].p[0].z=2.0f;
        r.p[1].p[1].x=2.0f;
        r.p[1].p[1].y=2.0f;
        r.p[1].p[1].z=-2.0f;
        r.p[1].p[2].x=2.0f;
        r.p[1].p[2].y=-2.0f;
        r.p[1].p[2].z=-2.0f;
        r.p[1].p[3].x=2.0f;
        r.p[1].p[3].y=-2.0f;
        r.p[1].p[3].z=2.0f;
        r.p[1].MoreTriangle(ndivisiones);//PointGenTriangle();

        //-------------square 3 left
        r.p[2].NewPoints(4);
        r.p[2].p[0].x=-2.0f;
        r.p[2].p[0].y=-2.0f;
        r.p[2].p[0].z=2.0f;
        r.p[2].p[1].x=2.0f;
        r.p[2].p[1].y=-2.0f;
        r.p[2].p[1].z=2.0f;
        r.p[2].p[2].x=2.0f;
        r.p[2].p[2].y=-2.0f;
        r.p[2].p[2].z=-2.0f;
        r.p[2].p[3].x=-2.0f;
        r.p[2].p[3].y=-2.0f;
        r.p[2].p[3].z=-2.0f;
        r.p[2].MoreTriangle(ndivisiones);//PointGenTriangle();

        //-------------square 4 right
        r.p[3].NewPoints(4);
        r.p[3].p[0].x=2.0f;
        r.p[3].p[0].y=2.0f;
        r.p[3].p[0].z=2.0f;
        r.p[3].p[1].x=-2.0f;
        r.p[3].p[1].y=2.0f;
        r.p[3].p[1].z=2.0f;
        r.p[3].p[2].x=-2.0f;
        r.p[3].p[2].y=2.0f;
        r.p[3].p[2].z=-2.0f;
        r.p[3].p[3].x=2.0f;
        r.p[3].p[3].y=2.0f;
        r.p[3].p[3].z=-2.0f;
        r.p[3].MoreTriangle(ndivisiones);//PointGenTriangle();

        //-------------square 5 top
        r.p[4].NewPoints(4);
        r.p[4].p[0].x=-2.0f;
        r.p[4].p[0].y=-2.0f;
        r.p[4].p[0].z=2.0f;
        r.p[4].p[1].x=-2.0f;
        r.p[4].p[1].y=2.0f;
        r.p[4].p[1].z=2.0f;
        r.p[4].p[2].x=2.0f;
        r.p[4].p[2].y=2.0f;
        r.p[4].p[2].z=2.0f;
        r.p[4].p[3].x=2.0f;
        r.p[4].p[3].y=-2.0f;
        r.p[4].p[3].z=2.0f;
        r.p[4].MoreTriangle(ndivisiones);//PointGenTriangle();

        //-------------square 1 bottom
        r.p[5].NewPoints(4);
        r.p[5].p[0].x=-2.0f;
        r.p[5].p[0].y=2.0f;
        r.p[5].p[0].z=-2.0f;
        r.p[5].p[1].x=-2.0f;
        r.p[5].p[1].y=-2.0f;
        r.p[5].p[1].z=-2.0f;
        r.p[5].p[2].x=2.0f;
        r.p[5].p[2].y=-2.0f;
        r.p[5].p[2].z=-2.0f;
        r.p[5].p[3].x=2.0f;
        r.p[5].p[3].y=2.0f;
        r.p[5].p[3].z=-2.0f;
        r.p[5].MoreTriangle(ndivisiones);//PointGenTriangle();

        //Inicializar normales de los planos
        // se calculan las normales con la normal de su primer triangulo
        // se calcula el baricentro de los tri�ngulos
        int cont_t=0;
        for (int i=0; i<r.NP; i++) {
            r.p[i].n=TriangleNormal(r.p[i].t[0]);
            for (int j=0;j<r.p[i].NT;j++){
                r.p[i].t[j].CalculateProjection();
                r.p[i].t[j].Centroid();
                r.p[i].t[j].ID=cont_t;
                cont_t++;
            }
        }
        numTri=cont_t;
        numRec=1;


    }
    else{
        cuartoCargado=false;
        r.Clear();
        numTri=0;
        if (simularPropagacion){
            simularPropagacion=false;
            Timer1->Enabled=false;
            distancia=0.0;
            tiempo=0.0;
            delete ry;
            delete s.Rays;
            delete r1.eR;
            ry=NULL;
            s.Rays=NULL;
            s.NRAYS=0;
            r1.eR=NULL;
            r1.NIt=0;
        }
    }
}

//---------------------------------------------------------------------------
void __fastcall TFormView::btnCalcularClick(TObject *Sender)
{
    if (simularPropagacion == false && cuartoCargado == true) {

        // Reiniciar los contadores de impactos antes de la nueva simulaci�n
        for (int i = 0; i < r.NP; i++) {
            for (int j = 0; j < r.p[i].NT; j++) {
                r.p[i].t[j].impactCounter = 0;  // Reiniciamos cada tri�ngulo
            }
        }

        double eneRay, eneRes;  // Energ�a del rayo y energ�a residual
        durSim = 1000;          // Duraci�n de la simulaci�n (1 seg o 1000 miliseg)
        r1.createTimeSamples(durSim);
        s.eF = 100;
        s.createRays(edRayos->Text.ToInt());
        edRayos->Text = s.NRAYS;

        eneRay = s.eF / s.NRAYS;

        ry = r.RayTracing(s.p, s.Rays, s.NRAYS);

        distancia = 0.0;
        tiempo = 0.0;
        simularPropagacion = true;
        Timer1->Enabled = true;
    } 
    else if (simularPropagacion == true) {
        simularPropagacion = false;
        Timer1->Enabled = false;
        distancia = 0.0;
        tiempo = 0.0;
        delete s.Rays;
        delete r1.eR;
        delete ry;
        ry = NULL;
        s.Rays = NULL;
        s.NRAYS = 0;
        r1.eR = NULL;
        r1.NIt = 0;
    }
}


//---------------------------------------------------------------------------
void __fastcall TFormView::FormMouseMove(TObject *Sender,
        TShiftState Shift, int X, int Y) {
    int dx,dy;
    int k;
    vector n,nplane;
    point i,m,z;
    double v;

    if(Shift.Contains(ssShift))
        v=4;
    else if(Shift.Contains(ssCtrl))
        v=2;
    else
        v=1;

    if(Shift.Contains(ssLeft) ) {
        dx = X-LastMousePos[0];//Negativo mueve a la derecha
        dy = Y-LastMousePos[1];//Negativo mueve a arriba

        //viewer rotation

        if(dx<0)
            a+=v*PI/60;
        else
            a-=v*PI/60;

        if(dy>0) {
            b+=v*PI/60;
        } else {
            b-=v*PI/60;
        }
    }

    LastMousePos[0] = X;
    LastMousePos[1] = Y;
}
//---------------------------------------------------------------------------

void __fastcall TFormView::FormMouseWheelDown(TObject *Sender,
      TShiftState Shift, TPoint &MousePos, bool &Handled)
{
    //viewer zoom
    double v;
    if(Shift.Contains(ssShift)) {
        v=4;
    } else if(Shift.Contains(ssCtrl)) {
        v=8;
    } else {
        v=2;
    }
    if(d<100*v*2*g)
        d+=v*2*g*tan(0.5/(v*2))/tan(f/(v*2));
}
//---------------------------------------------------------------------------

void __fastcall TFormView::FormMouseWheelUp(TObject *Sender,
      TShiftState Shift, TPoint &MousePos, bool &Handled)
{
//viewer zoom
    double v;
    if(Shift.Contains(ssShift)) {
        v=4;
    } else if(Shift.Contains(ssCtrl)) {
        v=8;
    } else {
        v=2;
    }
    if(d>v*2*g)
        d-=v*2*g*tan(0.5/(v*2))/tan(f/(v*2));   //0.5 is the initial fov
}
//---------------------------------------------------------------------------


void __fastcall TFormView::FormKeyDown(TObject *Sender, WORD &Key,
      TShiftState Shift)
{
    ShowMessage("Test");
}
//---------------------------------------------------------------------------

void __fastcall TFormView::chbTriangulosClick(TObject *Sender)
{
    if(verTriangulos){
        verTriangulos=false;
    }
    else{
        verTriangulos=true;
    }
}
//---------------------------------------------------------------------------



