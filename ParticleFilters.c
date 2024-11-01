/*
  CSC C85 - Fundamentals of Robotics and Automated Systems

  Particle filters implementation for a simple robot.

  Your goal here is to implement a simple particle filter
  algorithm for robot localization.

  A map file in .ppm format is read from disk, the map
  contains empty spaces and walls.

  A simple robot is randomly placed on this map, and
  your task is to write a particle filter algorithm to
  allow the robot to determine its location with
  high certainty.

  You must complete all sections marked

  TO DO:

  NOTE: 2 keyboard controls are provided:

  q -> quit (exit program)
  r -> reset particle set during simulation

  Written by F.J.E. for CSC C85, May 2012. U
  pdated Oct. 2021

  This robot model inspired by Sebastian Thrun's
  model in CS373.
*/

#include "ParticleFilters.h"
#include <math.h>
#include <stdbool.h>



/**********************************************************
 GLOBAL DATA
**********************************************************/
unsigned char *map;		// Input map
unsigned char *map_b;		// Temporary frame
struct particle *robot;		// Robot
struct particle *list;		// Particle list
int sx,sy;			// Size of the map image
char name[1024];		// Name of the map
int n_particles;		// Number of particles
int windowID;               	// Glut window ID (for display)
int Win[2];                 	// window (x,y) size
int RESETflag;			// RESET particles

int iterations;
bool localizationAchieved;

/**********************************************************
 PROGRAM CODE
**********************************************************/
int main(int argc, char *argv[])
{
 /*
   Main function. Usage for this program:

   ParticleFilters map_name n_particles

   Where:
    map_name is the name of a .ppm file containing the map. The map
             should be BLACK on empty (free) space, and coloured
             wherever there are obstacles or walls. Anythin not
             black is an obstacle.

    n_particles is the number of particles to simulate in [100, 50000]

   Main loads the map image, initializes a robot at a random location
    in the map, and sets up the OpenGL stuff before entering the
    filtering loop.
 */

 if (argc!=3)
 {
  fprintf(stderr,"Wrong number of parameters. Usage: ParticleFilters map_name n_particles.\n");
  exit(0);
 }

 strcpy(&name[0],argv[1]);
 n_particles=atoi(argv[2]);

 if (n_particles<100||n_particles>50000)
 {
  fprintf(stderr,"Number of particles must be in [100, 50000]\n");
  exit(0);
 }

 fprintf(stderr,"Reading input map\n");
 map=readPPMimage(name,&sx, &sy);
 if (map==NULL)
 {
  fprintf(stderr,"Unable to open input map, or not a .ppm file\n");
  exit(0);
 }

 // Allocate memory for the temporary frame
 fprintf(stderr,"Allocating temp. frame\n");
 map_b=(unsigned char *)calloc(sx*sy*3,sizeof(unsigned char));
 if (map_b==NULL)
 {
  fprintf(stderr,"Out of memory allocating image data\n");
  free(map);
  exit(0);
 }

//  srand48((long)time(NULL));		// Initialize random generator from timer
  srand48(12345);
 // CHANGE the line above to 'srand48(12345);'  to get a consistent sequence of random numbers for testing and debugging your code!
 

 // INITIALIZE the robot at a random location and orientation.
 iterations = 1;
 localizationAchieved = false;
 fprintf(stderr,"Init robot...\n");
 robot=initRobot(map,sx,sy);
 if (robot==NULL)
 {
  fprintf(stderr,"Unable to initialize robot.\n");
  free(map);
  free(map_b);
  exit(0);
 }
 sonar_measurement(robot,map,sx,sy);	// Initial measurements...

 // Initialize particles at random locations
 fprintf(stderr,"Init particles...\n");
 list=NULL;
 initParticles();

 // Done, set up OpenGL and call particle filter loop
 fprintf(stderr,"Entering main loop...\n");
 Win[0]=800;
 Win[1]=800;
 glutInit(&argc, argv);
 initGlut(argv[0]);
 glutMainLoop();

 // This point is NEVER reached... memory leaks must be resolved by OpenGL main loop
 exit(0);

}

void initParticles(void)
{
 /*
   This function creates and returns a linked list of particles
   initialized with random locations (not over obstacles or walls)
   and random orientations.

   There is a utility function to help you find whether a particle
   is on top of a wall.

   Use the global pointer 'list' to keep trak of the *HEAD* of the
   linked list.

   Probabilities should be uniform for the initial set.
 */

 list=NULL;

 /***************************************************************
 // TO DO: Complete this function to generate an initially random
 //        list of particles.
 ***************************************************************/
  srand(time(NULL));
  
 // Create and initialize n_particles
 for (int i = 0; i < n_particles; i++) {
     // Allocate memory for a new particle
     struct particle *newParticle = (struct particle*)malloc(sizeof(struct particle));
     
     int validPosition = 0;  // Flag to ensure valid particle placement

     // Keep trying until a valid position is found
     while (!validPosition) {
         // Randomly initialize the particle's x and y positions within map bounds
         newParticle->x = rand() % sx;
         newParticle->y = rand() % sy;

         // Use the hit function to check if this position is valid (not on a wall)
         if (!hit(newParticle, map, sx, sy)) {
             validPosition = 1;  // Valid position found
         }
     }

     // Randomly assign a heading/direction (theta) in degrees
     newParticle->theta = ((double)rand() / RAND_MAX) * 360.0;

     // Set the initial probability (uniform distribution across particles)
     newParticle->prob = 1.0 / n_particles;

     // Add the new particle to the front of the linked list
     newParticle->next = list;
     list = newParticle;
 }

}

void computeLikelihood(struct particle *p, struct particle *rob, double noise_sigma)
{
 /*
   This function computes the likelihood of a particle p given the sensor
   readings from robot 'robot'.

   Both particles and robot have a measurements array 'measureD' with 16
   entries corresponding to 16 sonar slices.

   The likelihood of a particle depends on how closely its measureD
   values match the values the robot is 'observing' around it. Of
   course, the measureD values for a particle come from the input
   map directly, and are not produced by a simulated sonar.
   
   To be clear: The robot uses the sonar_measurement() function to
    get readings of distance to walls from its current location
    
   The particles use the ground_truth() function to determine what
    distance the walls actually are from the location of the particle
    given the map.
    
   You then have to compare what the particles should be measuring
    with what the robot measured, and determine the appropriate
    probability of the difference between the robot measurements
    and what the particles expect to see.

   Assume each sonar-slice measurement is independent, and if

   error_i = (p->measureD[i])-(sonar->measureD[i])

   is the error for slice i, the probability of observing such an
   error is given by a Gaussian distribution with sigma=20 (the
   noise standard deviation hardcoded into the robot's sonar).

   You may want to check your numbers are not all going to zero...
   (this can happen easily when you multiply many small numbers
    together).

   This function updates the Belief B(p_i) for the particle 
   stored in 'p->prob'
 */

 /****************************************************************
 // TO DO: Complete this function to calculate the particle's
 //        likelihood given the robot's measurements
 ****************************************************************/

  double likelihood = 1.0;  // Start with likelihood 1

  for (int i = 0; i < 16; i++) {
      double error = p->measureD[i] - rob->measureD[i];  // Difference in measurements
      double probability = (1.0 / (sqrt(2 * M_PI) * noise_sigma)) *
                            exp(-(error * error) / (2 * noise_sigma * noise_sigma));

      // Multiply probabilities for each sonar slice
      likelihood *= probability;
  }

  // Set the particle's probability
  p->prob = likelihood;
}

void normalizeProbabilities(struct particle *list) {
    double total_prob = 0.0;
    struct particle *p = list;

    // Calculate total probability (sum of likelihoods)
    while (p != NULL) {
        total_prob += p->prob;
        p = p->next;
    }

    // Normalize each particle’s probability
    p = list;
    while (p != NULL) {
        if (total_prob > 0) {
            p->prob /= total_prob; // Normalize to convert likelihood to belief
        } else {
            p->prob = 1.0 / n_particles;  // Handle edge case if all probabilities are zero
        }
        p = p->next;
    }
}

struct particle *resample(void) {
    // construct a new list of particles
    struct particle *new_list = NULL;

    for (int i = 0; i < n_particles; i++) {
        double cumulative_prob = 0.0;
        double r = rand() / (double)RAND_MAX;  // Random number between 0 and 1

        struct particle *p = list;
        while (p != NULL) {
            cumulative_prob += p->prob;
            if (cumulative_prob >= r) {
                // Copy the particle to the new list
                struct particle *new_particle = (struct particle*)malloc(sizeof(struct particle));
                new_particle->x = p->x;
                new_particle->y = p->y;
                new_particle->theta = p->theta;
                new_particle->prob = 1.0 / n_particles;  // Initialize with uniform probability
                new_particle->next = new_list;
                new_list = new_particle;
                break;
            }
            p = p->next;
        }
    }

    deleteList(list);  // Free memory for the old list

    // uniformly randomize upto 5% of the particles (less if higher iterations)
    int num_random = n_particles * 0.05 * (1.0 / (iterations/100.0));
    for (int i = 0; i < num_random; i++) {
        struct particle *p = new_list;
        int random_particle = rand() % n_particles;
        for (int j = 0; j < random_particle; j++) {
            p = p->next;
        }

        int validPosition = 0;
        while(!validPosition) {
            p->x = rand() % sx;
            p->y = rand() % sy;
            p->theta = ((double)rand() / RAND_MAX) * 360.0;
            if (!hit(p, map, sx, sy)) {
                validPosition = 1;
            }
        }
    }

    return new_list;
}

bool isCentralized(struct particle *list, double threshold) {
    double mean_x = 0.0, mean_y = 0.0;
    int count = 0;
    struct particle *p = list;

    // Calculate mean position of particles
    while (p != NULL) {
        mean_x += p->x;
        mean_y += p->y;
        count++;
        p = p->next;
    }
    mean_x /= count;
    mean_y /= count;

    // Calculate variance
    double variance_x = 0.0, variance_y = 0.0;
    p = list;
    while (p != NULL) {
        variance_x += (p->x - mean_x) * (p->x - mean_x);
        variance_y += (p->y - mean_y) * (p->y - mean_y);
        p = p->next;
    }
    variance_x /= count;
    variance_y /= count;

    // Check if variances are below the threshold (indicating centralization)
    // printf("Variance x: %f, Variance y: %f\n", variance_x, variance_y);
    return (variance_x < threshold && variance_y < threshold);
}

void ParticleFilterLoop(void)
{
 /*
    Main loop of the particle filter
 */

  // OpenGL variables. Do not remove
  unsigned char *tmp;
  GLuint texture;
  static int first_frame=1;
  double max;
  struct particle *p,*pmax;
  char line[1024];

  iterations += n_particles / 1000;  // Increase iterations by 1 every 1000 particles
  // Add any local variables you need right below.

  if (!first_frame)
  {
   // Step 1 - Move all particles a given distance forward (this will be in
   //          whatever direction the particle is currently looking).
   //          To avoid at this point the problem of 'navigating' the
   //          environment, any particle whose motion would result
   //          in hitting a wall should be bounced off into a random
   //          direction.
   //          Once the particle has been moved, we call ground_truth(p)
   //          for the particle. This tells us what we would
   //          expect to sense if the robot were actually at the particle's
   //          location.
   //
   //          Don't forget to move the robot the same distance!

   /******************************************************************
   // TO DO: Complete Step 1 and test it
   //        You should see a moving robot and sonar figure with
   //        a set of moving particles.
   ******************************************************************/
    struct particle *p = list;
    double move_distance = 1.0; // Define a small move distance for each particle

    while (p != NULL) {
        // Move the particle forward
        move(p, move_distance);

        // Check if particle hits a wall, and "bounce" if so
        // if (hit(p, map, sx, sy)) {
        //     // Assign a random direction if it hits a wall
        //     p->theta = ((double)rand() / RAND_MAX) * 360.0;
        // }

        if(hit(p, map, sx, sy)) {
            int validTheta = 0;
            while (!validTheta) {
                double oldx = p->x;
                double oldy = p->y;
                p->theta = ((double)rand() / RAND_MAX) * 360.0;
                move(p, move_distance);
                if (!hit(p, map, sx, sy)) {
                    validTheta = 1;
                } else {
                    p->x = oldx;
                    p->y = oldy;
                }
            }
        }

        // Update the particle's expected measurement (ground truth)
        ground_truth(p, map, sx, sy);

        // Move to the next particle in the list
        p = p->next;
    }

    // Move the robot forward the same distance
    move(robot, move_distance);
    
    if (hit(robot, map, sx, sy)) {
        int validTheta = 0;
        while (!validTheta) {
            double oldx = robot->x;
            double oldy = robot->y;
            robot->theta = ((double)rand() / RAND_MAX) * 360.0;
            move(robot, move_distance);
            if (!hit(robot, map, sx, sy)) {
                validTheta = 1;
            } else {
                robot->x = oldx;
                robot->y = oldy;
            }
        }
    }

    // // Update the robot's actual sonar measurement
    // sonar_measurement(robot, map, sx, sy);
    

   // Step 2 - The robot makes a measurement - use the sonar
   sonar_measurement(robot,map,sx,sy);

   // Step 3 - Compute the likelihood for particles based on the sensor
   //          measurement. See 'computeLikelihood()' and call it for
   //          each particle. Once you have a likelihood for every
   //          particle, turn it into a probability by ensuring that
   //          the sum of the likelihoods for all particles is 1.

   /*******************************************************************
   // TO DO: Complete Step 3 and test it
   //        You should see the brightness of particles change
   //        depending on how well they agree with the robot's
   //        sonar measurements. If all goes well, particles
   //        that agree with the robot's position/direction
   //        should be brightest.
   *******************************************************************/

  // Step 3: Compute the likelihood for each particle
  //struct particle *p = list;
p = list; // Start from the head of the list

while (p != NULL) {
    // Calculate the likelihood for each particle based on the robot's measurement
    computeLikelihood(p, robot, 20.0); // Assume noise_sigma = 20

    // Move to the next particle in the list
    p = p->next;
}

// Now normalize all likelihoods to convert them to beliefs
normalizeProbabilities(list);
   // Step 4 - Resample particle set based on the probabilities. The goal
   //          of this is to obtain a particle set that better reflect our
   //          current belief on the location and direction of motion
   //          for the robot. Particles that have higher probability will
   //          be selected more often than those with lower probability.
   //
   //          To do this: Create a separate (new) list of particles,
   //                      for each of 'n_particles' new particles,
   //                      randomly choose a particle from  the current
   //                      set with probability given by the particle
   //                      probabilities computed in Step 3.
   //                      Initialize the new particles (x,y,theta)
   //                      from the selected particle.
   //                      Note that particles in the current set that
   //                      have high probability may end up being
   //                      copied multiple times.
   //
   //                      Once you have a new list of particles, replace
   //                      the current list with the new one. Be sure
   //                      to release the memory for the current list
   //                      before you lose that pointer!
   //

   /*******************************************************************
   // TO DO: Complete and test Step 4
   //        You should see most particles disappear except for
   //        those that agree with the robot's measurements.
   //        Hopefully the largest cluster will be on and around
   //        the robot's actual location/direction.
   *******************************************************************/
  list = resample();

  // need to figure out if we achieved localization    
  if (!localizationAchieved && isCentralized(list, 100)) {  // Assume 1.0 is the threshold for centralization
        localizationAchieved = true;  // Set the flag to stop the loop
        fprintf(stderr, "I found myself!\n");
        // return;
  }

  }  // End if (!first_frame)

  /***************************************************
   OpenGL stuff
   You DO NOT need to read code below here. It only
   takes care of updating the screen.
  ***************************************************/
  if (RESETflag)	// If user pressed r, reset particles
  {
   deleteList(list);
   list=NULL;
   initParticles();
   RESETflag=0;
  }
  renderFrame(map,map_b,sx,sy,robot,list);

  // Clear the screen and depth buffers
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glEnable(GL_TEXTURE_2D);
  glDisable(GL_LIGHTING);

  glGenTextures( 1, &texture);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glBindTexture( GL_TEXTURE_2D, texture);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  glTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);

  glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB, sx, sy, 0, GL_RGB, GL_UNSIGNED_BYTE, map_b);

  // Draw box bounding the viewing area
  glBegin (GL_QUADS);
  glTexCoord2f (0.0, 0.0);
  glVertex3f (0.0, 100.0, 0.0);
  glTexCoord2f (1.0, 0.0);
  glVertex3f (800.0, 100.0, 0.0);
  glTexCoord2f (1.0, 1.0);
  glVertex3f (800.0, 700.0, 0.0);
  glTexCoord2f (0.0, 1.0);
  glVertex3f (0.0, 700.0, 0.0);
  glEnd ();

  p=list;
  max=0;
  while (p!=NULL)
  {
   if (p->prob>max)
   {
    max=p->prob;
    pmax=p;
   }
   p=p->next;
  }

  if (!first_frame)
  {
   sprintf(&line[0],"X=%3.2f, Y=%3.2f, th=%3.2f, EstX=%3.2f, EstY=%3.2f, Est_th=%3.2f, Error=%f",robot->x,robot->y,robot->theta,\
           pmax->x,pmax->y,pmax->theta,sqrt(((robot->x-pmax->x)*(robot->x-pmax->x))+((robot->y-pmax->y)*(robot->y-pmax->y))));
   glColor3f(1.0,1.0,1.0);
   glRasterPos2i(5,22);
   for (int i=0; i<strlen(&line[0]); i++)
    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18,(int)line[i]);
  }

  // Make sure all OpenGL commands are executed
  glFlush();
  // Swap buffers to enable smooth animation
  glutSwapBuffers();

  glDeleteTextures( 1, &texture );

  // Tell glut window to update ls itself
  glutSetWindow(windowID);
  glutPostRedisplay();

  if (first_frame)
  {
   fprintf(stderr,"All set! press enter to start\n");
   //gets(&line[0]);
   if (fgets(line, sizeof(line), stdin) == NULL) {
    fprintf(stderr, "Error reading input.\n");
    exit(1);
}


   first_frame=0;
  }
}

/*********************************************************************
 OpenGL and display stuff follows, you do not need to read code
 below this line.
*********************************************************************/
// Initialize glut and create a window with the specified caption
void initGlut(char* winName)
{
    // Set video mode: double-buffered, color, depth-buffered
    glutInitDisplayMode (GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);

    // Create window
    glutInitWindowPosition (0, 0);
    glutInitWindowSize(Win[0],Win[1]);
    windowID = glutCreateWindow(winName);

    // Setup callback functions to handle window-related events.
    // In particular, OpenGL has to be informed of which functions
    // to call when the image needs to be refreshed, and when the
    // image window is being resized.
    glutReshapeFunc(WindowReshape);   // Call WindowReshape whenever window resized
    glutDisplayFunc(ParticleFilterLoop);   // Main display function is also the main loop
    glutKeyboardFunc(kbHandler);
}

void kbHandler(unsigned char key, int x, int y)
{
 if (key=='r') {RESETflag=1;}
 if (key=='q') {deleteList(list); free(map); free(map_b); exit(0);}
}

void WindowReshape(int w, int h)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();			// Initialize with identity matrix
    gluOrtho2D(0, 800, 800, 0);
    glViewport(0,0,w,h);
    Win[0] = w;
    Win[1] = h;
}
