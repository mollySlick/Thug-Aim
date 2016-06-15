/* An example of the minimal Win32 & OpenGL program.  It only works in
   16 bit color modes or higher (since it doesn't create a
   palette). */



#include <windows.h>			/* must include this before GL/gl.h */
#include <windowsx.h>
#include <GL/gl.h>			/* OpenGL header file */
#include <GL/glu.h>			/* OpenGL utilities header file */
#include <stdio.h>
#include <time.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define bool unsigned char
#define true 1
#define false 0

struct timespec t;

#define SCRW 800
#define SCRH 600

// Epsilon for floating point comparisons
float fps = 0.0000000001f;

bool Keys[256];
double MousePos[2];
bool MouseBtns[3]; // left, right, middle
typedef struct Entity
{
	double X;
	double Y;
	double VelocityX;
	double VelocityY;
	char* name;
	long index;
	float drawsize;
	bool hasfocus;
	char* parentname;
	struct Entity* parent;
	double speed;
	int visiondistance;
	double** vision;
	bool deleted;
	int lastfiretime;
	int fireinterval;
	
} Entity;

typedef struct Mission
{
	int MissionNumber;
	int NumberOfKills;
} Mission;
/***/

GLuint LoadGLBMP(const char* filename)
{
	// Data read from the header of the BMP file
	unsigned char header[54]; // Each BMP file begins by a 54-bytes header
	unsigned int dataPos;     // Position in the file where the actual data begins
	unsigned int width, height;
	unsigned int imageSize;   // = width*height*3
	// Actual RGB data
	unsigned char * data;

	FILE * file = fopen(filename,"rb");
	if (!file){printf("Could not find %s\r\n", filename); return 0;}

	if ( fread(header, 1, 54, file)!=54  || ( header[0]!='B' || header[1]!='M' )){ // If not 54 bytes read : problem
    printf("%s not a bitmap.\r\n", filename);
    return false;
}


	// Read ints from the byte array
	dataPos    = *(int*)&(header[0x0A]);
	imageSize  = *(int*)&(header[0x22]);
	width      = *(int*)&(header[0x12]);
	height     = *(int*)&(header[0x16]);

	// Some BMP files are misformatted, guess missing information
	if (imageSize==0)    imageSize=width*height*3; // 3 : one byte for each Red, Green and Blue component
	if (dataPos==0)      dataPos=54; // The BMP header is done that way

	// Create a buffer
	data = (unsigned char*)malloc(imageSize);

	// Read the actual data from the file into the buffer
	fread(data,1,imageSize,file);

	//Everything is in memory now, the file can be closed
	fclose(file);

	// Create one OpenGL texture
	GLuint textureID;
	glGenTextures(1, &textureID);

	// "Bind" the newly created texture : all future texture functions will modify this texture
	glBindTexture(GL_TEXTURE_2D, textureID);

	// Convert BGR TO RGB
	/*
	for (int i = 0; i < imageSize*3; i+=3)
	{
		data[i] = data[i] ^ data[i+2];
		data[i+2] = data[i+2] ^ data[i];
		data[i] = data[i] ^ data[i+2];
	}
	*/
	// Give the image to OpenGL
	glTexImage2D(GL_TEXTURE_2D, 0,GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	free(data);


	return textureID;
}


// Truncates Entity array and returns new size
int TruncateEntityArray(Entity** a, int* sz)
{
	Entity** tmp;
	int newsz = 0;
	for (int i = 0; i < *sz; i++)
	{
		if (a[i] != NULL) newsz++;
	}
	tmp = (Entity**)malloc(newsz * sizeof(Entity*));

	for (int i = 0; i < *sz; i++)
	{
		if (a[i] != NULL)
		{
			tmp[i] = a[i];
		}
	}
	a = tmp;
	*sz = newsz;
	return newsz;
}

/*...*/

void array_push(void** array, int arraysize, void* element, int elementsize)
{
	array = realloc(array, arraysize + elementsize);
	array[sizeof(void**)*arraysize] = element;
}

float d = 0.1f;


double GetMilliseconds(double cpu_ticks)
{
	return ((cpu_ticks * 1000.0f) / CLOCKS_PER_SEC);
}
double ElapsedMilliseconds()
{
    return ((clock() * 1000.0f) / CLOCKS_PER_SEC);
}

HDC hDC;				/* device context */
HPALETTE hPalette = 0;			/* custom palette (if needed) */
GLboolean animate = GL_TRUE;		/* animation flag */
unsigned char mainloop = 1;

Entity** ents;
int numents = 0;
Entity* player;

Entity playerprojectiles[128]; // that'll do, pig
int numpp = 128;

GLuint fontID;

void SpawnEnemyRandom()
{
	Entity* enemy = malloc(sizeof(Entity));

	/* Spawns an enemy outside of the screen */
	int quadrant = rand() % 4;

	switch(quadrant)
	{
		case 0: // to the right
		{
			enemy->X = player->X + SCRW / 2;
			enemy->Y = rand() % SCRH;
			break;
		}
		case 1: // Above
		{
			enemy->X = rand() % SCRH - SCRW / 2;
			enemy->Y = player->Y - SCRH / 2;
			break;
		}
		case 2: // to the left
		{
			enemy->X = player->X - SCRW / 2;
			enemy->Y = rand() % SCRH;
			break;
		}
		case 3: // below
		{
			enemy->X = rand() % SCRW;
			enemy->Y = player->Y + SCRH / 2;
			break;
		}
	}

		
		enemy->name = "enemy";
		enemy->index = clock();
		enemy->drawsize = 15.0f;
		enemy->hasfocus = false;
		enemy->speed = 0.2f;
		/*
		enemy->visiondistance = 10;
		// Sight
		enemy->vision = (double**)malloc(sizeof(double*) * enemy->visiondistance);
		for (int i = 0; i < enemy->visiondistance; i++)
		{
			enemy->vision[i] = (double*)malloc(sizeof(double*) * enemy->visiondistance);
		}
		*/
		ents = (Entity**)realloc(ents, (numents+1)*sizeof(Entity));
		ents[numents] = enemy;
		
		numents++;
}

void initialize()
{
	srand(time(0) % clock());
	ents = malloc(0);

	player = malloc(sizeof(Entity));
	player->X = 0;
	player->Y = 0;
	player->name = "player";
	player->index = clock();
	player->drawsize = 15.0f;
	player->hasfocus = true;
	player->fireinterval = 240;
	player->lastfiretime = 0;
	ents = (Entity**)realloc(ents, (numents+1)*sizeof(Entity));
	ents[numents] = player;
	numents++;
	for (int i = 0; i < 5; i++)
	{
		SpawnEnemyRandom();
	}

	for (int i = 0; i < numpp; i++)
	{
		playerprojectiles[i].deleted = true;
	}

	fontID = LoadGLBMP("font.bmp");
}



void SpawnPlayerProjectile(Entity* player)
{
	Entity pp;
	pp.X = player->X;
	pp.Y = player->Y;
	pp.drawsize = 3.5f;
	pp.speed = 1.8f;
	pp.name = "player_projectile";
	pp.index = clock();
	pp.hasfocus = false;

	float speedx = pp.speed;
	float speedy = pp.speed;
	
	float dx = MousePos[0] - (player->X + SCRW / 2) + player->X;
	float dy =  MousePos[1] - (player->Y  + SCRH / 2) + player->Y;

	float mag = sqrt(dx*dx + dy*dy);
	if (mag > fps)
	{
		speedx = dx*speedx / mag;
		speedy = dy*speedy / mag;
	}
	
	pp.VelocityX = speedx;
	pp.VelocityY = speedy;
	pp.deleted = false;

	for (int i = 0; i < numpp; i++)
	{
		if (playerprojectiles[i].deleted)
		{
			playerprojectiles[i] = pp;
			break;
		}
	}
}

long int ticks = 0;
void update(double delta)
{
		d += 0.03f * delta;

		static float playerspeed = 0.44f;

		float x = 0;
		float y = 0;

		if (MouseBtns[0] && GetTickCount() - player->fireinterval > player->lastfiretime)
		{
				SpawnPlayerProjectile(player);
				player->lastfiretime = GetTickCount();
		}

		if (Keys['W'])
		{
			y = -1;
			//player->Y -= playerspeed;
		}
		if (Keys['S'])
		{
			y = 1;
			//payer->Y += playerspeed;
		}
		if (Keys['A'])
		{
			x = -1;
			//player->X -= playerspeed;
		}
		if (Keys['D'])
		{
			x = 1;
			//player->X += playerspeed;
		}

		float magnitude = sqrt(x*x + y*y);
		if (magnitude > fps)
		{
			x /= magnitude;
			y /= magnitude;

			player->X += x * playerspeed;
			player->Y += y * playerspeed;
		}

		// Apply velocity toward the pointer of speed.
		for (int i = 0; i < numpp; i++)
		{
			if (playerprojectiles[i].deleted) continue;

			playerprojectiles[i].X += playerprojectiles[i].VelocityX;
			playerprojectiles[i].Y += playerprojectiles[i].VelocityY;			

			if (playerprojectiles[i].X < player->X - SCRW / 2 || playerprojectiles[i].X > player->X + SCRW / 2 ||
				playerprojectiles[i].Y < player->Y - SCRH / 2 || playerprojectiles[i].Y > player->Y + SCRH / 2 )
			{
				playerprojectiles[i].deleted = true;
			}
		}


		for (int i = 0; i < numents; i++)
		{
			if (ents[i] == NULL) continue;

			if (strcmp(ents[i]->name, "enemy") == 0)
			{
				
				for (int j = 0; j < numpp; j++)
				{
					if (playerprojectiles[j].deleted) continue;

					float dx = playerprojectiles[j].X - ents[i]->X;
					float dy = playerprojectiles[j].Y - ents[i]->Y;
					float dst = sqrt(dx*dx + dy*dy);

					if (dst < ents[i]->drawsize)
					{
						free(ents[i]);
						playerprojectiles[j].deleted = true;
						ents[i] = NULL;
						break;
					}
				}

				if (ents[i] == NULL) continue;

				float speedx = ents[i]->speed;
				float speedy = ents[i]->speed;
				float dx = ents[i]->X - player->X;
				float dy = ents[i]->Y - player->Y;

				float mag = sqrt(dx*dx + dy*dy);
				if (abs(mag) > fps)
				{
				speedx = dx*speedx / mag;
				speedy = dy*speedy / mag;
				}
				ents[i]->X -= speedx;
				ents[i]->Y -= speedy;

				// Perception
				/*
				for (int i = 0; i < ents[i]->visiondistance; i++)
				{
					for (int j = 0; j < ents[i]->visiondistance; j++)
					{
						for (int k = 0; k < numents; k++)
						{
							if (ents[k]->index == ents[i]->index)
							{
								float dst = sqrt(dx*dx + dy*dy);
							}
						}d
					}
				}
				*/
			}
		}


		ticks++;
}

void
display()
{
	glPushMatrix();

    /* rotate a triangle around */
    glClear(GL_COLOR_BUFFER_BIT);

    glColor3f(1.0f, 1.0f, 1.0f);
    
    for (int i = 0; i < numents; i++)
    {
    	if (ents[i] == NULL) continue;
    	glPointSize(ents[i]->drawsize);
    	if (ents[i]->hasfocus)
    	{
    		glTranslatef(-ents[i]->X + SCRW / 2, -ents[i]->Y + SCRH / 2, 0);
    	}

    	glBegin(GL_POINTS);
    		glVertex3f(ents[i]->X, ents[i]->Y, 0);
    	glEnd();
    }

    // Grid
	 glPointSize(0.2f);
	 int sz = 50;
	 for (int i = 0; i < sz; i++)
	 {
	 	glBegin(GL_LINES);

	 	glVertex3f(i * sz, 0, 0);
	 	glVertex3f(i * sz, sz*sz, 0);

	 	for (int i = 0; i < sz; i++)
		 {
		 	glVertex3f(0, i * sz, 0);
		 	glVertex3f(sz*sz, i * sz, 0);
		 }

	 	glEnd();
	 }

   

    //playerprojectiles[numpp]
	for (int i = 0; i < numpp; i++)
	{
		if (playerprojectiles[i].deleted) continue;
		glPointSize(playerprojectiles[i].drawsize);
		glBegin(GL_POINTS);
			glVertex3f(playerprojectiles[i].X, playerprojectiles[i].Y, 0);		
		glEnd();
	}

	 glPopMatrix();

	double x = MousePos[0];
	double y = MousePos[1];
	int pts = 8;
	double reticlesize = 6.0f;
	static double r = 0;
	r += 0.003f;
	glPointSize(1.0f);
    // Reticle
    glBegin(GL_POINTS);

    glColor3f(1.0, 1.0f, 0.0f);
	for (int i = 0; i < pts; i++)
	{
			double d = 2*M_PI / (double)pts;
			d *= (double)i;
			glVertex3f(x + cos(d+r) * reticlesize,y + sin(d+r) * reticlesize, 0);
	}
	glColor3f(1.0f, 0, 1.0f);
	for (int i = 0; i < pts; i++)
	{
			double d = 2*M_PI / (double)pts;
			d *= (double)i;
			glVertex3f(x + cos(d-r) * reticlesize,y + sin(d-r) * reticlesize, 0);
	}
	glColor3f(0.0f, 1.0f, 1.0f);
	for (int i = 0; i < pts; i++)
	{
			double d = 2*M_PI / (double)pts;
			d *= (double)i - r;
			double rs = reticlesize * (sin(r) + 0.5f)* 1.2f ;
			glVertex3f(x + cos(d-r) * rs, y + sin(d-r) * rs, 0);
	}

    glEnd();


    // Draw text
    glColor3f(1, 1, 1);
    glBindTexture(GL_TEXTURE_2D, fontID);
    glBegin(GL_QUADS);

    glVertex3f(0, 0, 0);
    glVertex3f(50, 0, 0);
    glVertex3f(50, 50, 0);
    glVertex3f(0, 50, 0);

    glTexCoord2f (0, 0);
    glTexCoord2f (1, 0);
    glTexCoord2f (1, 1);
    glTexCoord2f (0, 1);

    glEnd();
 
    glFlush();
}


LONG WINAPI
WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{ 
    static PAINTSTRUCT ps;

    switch(uMsg) {


    case WM_CLOSE:
    mainloop = 0;
	PostQuitMessage(0);
	return 0;

	case WM_KEYDOWN:
	{
		if (wParam < 256)
		Keys[wParam] = true;
		break;
	}
	case WM_KEYUP:
	{
		if (wParam < 256)
		Keys[wParam] = false;
		break;
	}
	case WM_LBUTTONDOWN:
	{
		MouseBtns[0] = true;
		break;
	}
	case WM_LBUTTONUP:
	{
		MouseBtns[0] = false;
		break;
	}
	case WM_RBUTTONDOWN:
	{
		MouseBtns[1] = true;
	}
	case WM_RBUTTONUP:
	{
		MouseBtns[1] = false;
	}

    }


    return DefWindowProc(hWnd, uMsg, wParam, lParam); 
} 

HWND
CreateOpenGLWindow(char* title, int x, int y, int width, int height, 
		   BYTE type, DWORD flags)
{
    int         pf;
    HDC         hDC;
    HWND        hWnd;
    WNDCLASS    wc;
    PIXELFORMATDESCRIPTOR pfd;
    static HINSTANCE hInstance = 0;

    /* only register the window class once - use hInstance as a flag. */
    if (!hInstance) {
	hInstance = GetModuleHandle(NULL);
	wc.style         = CS_OWNDC;
	wc.lpfnWndProc   = (WNDPROC)WindowProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInstance;
	wc.hIcon         = LoadIcon(NULL, IDI_WINLOGO);
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName  = NULL;
	wc.lpszClassName = "Swarm";

	if (!RegisterClass(&wc)) {
	    MessageBox(NULL, "RegisterClass() failed:  "
		       "Cannot register window class.", "Error", MB_OK);
	    return NULL;
	}
    }

    hWnd = CreateWindow("Swarm", title, WS_OVERLAPPEDWINDOW |
			WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
			CW_USEDEFAULT, CW_USEDEFAULT, SCRW, SCRH, NULL, NULL, hInstance, NULL);

    if (hWnd == NULL) {
	MessageBox(NULL, "CreateWindow() failed:  Cannot create a window.",
		   "Error", MB_OK);
	return NULL;
    }

    hDC = GetDC(hWnd);

    /* there is no guarantee that the contents of the stack that become
       the pfd are zeroed, therefore _make sure_ to clear these bits. */
    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize        = sizeof(pfd);
    pfd.nVersion     = 1;
    pfd.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | flags;
    pfd.iPixelType   = type;
    pfd.cColorBits   = 32;

    pf = ChoosePixelFormat(hDC, &pfd);
    if (pf == 0) {
	MessageBox(NULL, "ChoosePixelFormat() failed:  "
		   "Cannot find a suitable pixel format.", "Error", MB_OK); 
	return 0;
    } 
 
    if (SetPixelFormat(hDC, pf, &pfd) == FALSE) {
	MessageBox(NULL, "SetPixelFormat() failed:  "
		   "Cannot set format specified.", "Error", MB_OK);
	return 0;
    } 

    DescribePixelFormat(hDC, pf, sizeof(PIXELFORMATDESCRIPTOR), &pfd);

    ReleaseDC(hWnd, hDC);

    return hWnd;
}    

int APIENTRY
WinMain(HINSTANCE hCurrentInst, HINSTANCE hPreviousInst,
	LPSTR lpszCmdLine, int nCmdShow)
{
    HDC hDC;				/* device context */
    HGLRC hRC;				/* opengl context */
    HWND  hWnd;				/* window */
    MSG   msg;				/* message */

    hWnd = CreateOpenGLWindow("Swarm", 0, 0, SCRW, SCRH, PFD_TYPE_RGBA, 0);
    if (hWnd == NULL)
	exit(1);

    hDC = GetDC(hWnd);
    hRC = wglCreateContext(hDC);
    wglMakeCurrent(hDC, hRC);

    initialize();

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    glOrtho(0, SCRW, SCRH, 0, 0, 0.1f);

    double endt = 0;
    double startt = 0;

	ShowCursor(FALSE);
    while (mainloop) {

    startt = ElapsedMilliseconds();
	
	
	while(PeekMessage(&msg, hWnd, 0, 0, PM_NOREMOVE)) {
	    if(GetMessage(&msg, hWnd, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	    } else {
		break;
	    }
	}

	// cursor
	POINT* p;
	GetCursorPos(p);
	ScreenToClient(hWnd, p);
	MousePos[0] = p->x;
	MousePos[1] = p->y * (SCRW / SCRH);


	display();
	update(1);

	Sleep(1);
    }

	wglMakeCurrent(NULL, NULL);
    ReleaseDC(hWnd, hDC);
    wglDeleteContext(hRC);
    DestroyWindow(hWnd);

    for (int i = 0; i < numents; i++)
    {
    	free(ents[i]);
    }
    free(ents);

    glDeleteTextures(1, &fontID);

    return msg.wParam;
}