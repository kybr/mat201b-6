/******************************************************************
MAT 201B, 2018 Winter
Author: Chengyuan Xu, cxu@ucsb.edu

Reference:
Based on starter code by Karl Yerkes
Flocking based on The Nature of Code by Daniel Shiffman
textured_background.cpp by Tim Wood

License: GPL-3.0
******************************************************************/

#include <float.h>
#include <math.h>
#include <cassert>  // gets you assert()
#include "Cuttlebone/Cuttlebone.hpp"
#include "Gamma/Oscillator.h"
#include "allocore/io/al_App.hpp"
#include "allocore/math/al_Ray.hpp"
#include "allocore/math/al_Vec.hpp"
#include "common.hpp"

using namespace al;
using namespace std;

#define TAIL_LENGTH 20
#define OUT_BOUND 200

// some of these must be carefully balanced; i spent some time turning them.
// change them however you like, but make a note of these settings.
unsigned fishCount = 300;      // try 2, 5, 50, and 5000
double maxAcceleration = 300;  // prevents explosion, loss of particles
double maxSpeed = 100;         // mock number
double initRadius = 50;        // initial condition
double initSpeed = 100;        // initial condition
double timeStep = 0.01;        // keys change this value for effect
double scaleFactor = 0.1;      // resizes the entire scene
double sphereRadius = 2;       // increase this to make collisions more frequent
int targetFishID = 0;

// global variables for sound
float targetToNav;
float nearbyFish;
float myFrameRate;

// helper function: makes a random vector
Vec3f r() { return Vec3f(rnd::uniformS(), rnd::uniformS(), rnd::uniformS()); }
Vec3f circle() {
  return Vec3f(
      initRadius * sin(rnd::uniformS(2 * M_PI)) * cos(rnd::uniformS(2 * M_PI)),
      initRadius * sin(rnd::uniformS(2 * M_PI)) * sin(rnd::uniformS(2 * M_PI)),
      initRadius * cos(rnd::uniformS(2 * M_PI)));
}

// global mesh variables
Mesh userFishMesh;
Mesh fishMeshS;
Mesh fishMeshM;
Mesh fishMeshL;
Mesh planktonMesh;

// Plankton
struct Plankton {
  float lifespan, mass;
  Vec3f position, velocity, acceleration;
  Color color;
  bool alive;
  int id;

  // *particles is the pointer to the actual particleList
  vector<Plankton> *planktons;

  Plankton() {}

  Plankton(vector<Plankton> *p, int i) {
    position = rnd::ball<Vec3f>() * 10.0f * initRadius;
    velocity = Vec3f(0, 0, 0);
    acceleration = Vec3f(0, 0, 0);
    planktons = p;
    alive = true;
    id = i;

    planktonMesh.primitive(Graphics::POINTS);
    planktonMesh.vertex(position.x, position.y, position.z);
    planktonMesh.color(255, 255, 255, 1);
  }

  void update() {
    if (alive == false) {
      planktonMesh.vertex(0, 100000, 0);
    } else {
      // Vec3f sep = separate();
      Vec3f stay = stayInCircle();

      // sep = sep * 1.5f;
      stay = stay * 0.1f;

      // applyForce(sep);
      applyForce(stay);

      velocity += acceleration * timeStep;
      position += velocity * timeStep;

      planktonMesh.vertex(position.x, position.y, position.z);

      acceleration.zero();  // reset acceleration after each update
    }
  }

  void draw(Graphics &g) { g.draw(planktonMesh); }

  Vec3f separate() {
    int count = 0;
    Vec3f steer;

    for (auto other : *planktons) {
      // this difference is a vector from b to a, put this force on a, so
      // push away.
      Vec3f difference = (position - other.position);
      float d = difference.mag();
      // if agents is getting closer, push away
      if (d > 0 && d < 20 * sphereRadius) {
        steer += difference.normalize() / d;
        count++;
      }
    }
    if (count > 0) {
      steer = steer / count;
    }
    if (steer.mag() > 0) {
      steer = steer.normalize() * maxSpeed;
      steer -= velocity;
    }
    return steer;
  }

  Vec3f seek(Vec3f target) {
    Vec3f desired = target - position;
    Vec3f steer = desired.normalize(maxSpeed);
    steer -= velocity;
    return steer;
  }

  void eaten() { alive = false; }

  Vec3f stayInCircle() { return seek(rnd::ball<Vec3f>() * 20.0f * initRadius); }

  void applyForce(Vec3f force) { acceleration += force; }
};

// NormalFish
struct Fish {
  float lifespan, mass, targetP_diff;
  Vec3f velocity, acceleration;
  Pose pose, target;
  Color color;
  bool alive;
  int id;

  // for target plankton
  int targetID;

  // *particles is the pointer to the actual particleList
  vector<Fish> *fishes;

  Fish() {}

  // using *p here because we don't want to copy the actual particleList every
  // time creating an instance, so using a pointer here
  Fish(vector<Fish> *f, int i) {
    // pose.pos() = circle() * 5;
    pose.pos() = (rnd::ball<Vec3f>() * 5 * initRadius);
    velocity = Vec3f(0, 0, 0).cross(pose.pos()).normalize(initSpeed);
    // velocity = Vec3f(0, 0, 0);
    // acceleration = r() * initSpeed;
    acceleration = Vec3f(0, 0, 0);
    color = HSV(rnd::uniform(), 1, 1);
    // pointing the *p to *particles so we can access the actual vector
    // via *p, by accessing *particles
    fishes = f;
    alive = true;
    id = i;
  }

  void update() {
    if (alive == false) {
      pose.pos() = Vec3f(0, 0, 10000);
    } else {
      // since *particles is inside class, no need to bring into the
      // functions.
      Vec3f sep = separate();
      Vec3f ali = align();
      Vec3f coh = cohesion();
      Vec3f stay = stayInCircle();

      // 4 * sphereRadius, 10 * sphereRadius, 30 * sphereRadius
      // 1.0 , 1.0 , 1.0 is an interesting stable combination

      sep = sep * 2.5f;
      ali = ali * 2.0f;
      coh = coh * 1.0f;
      stay = stay * 1.0f;

      applyForce(sep);
      applyForce(ali);
      applyForce(coh);
      // applyForce(stay);

      velocity += acceleration * timeStep;
      pose.pos() += velocity * timeStep;

      acceleration.zero();  // reset acceleration after each update
      targetP_diff = 10000.f;
    }
  }

  void draw(Graphics &g) {
    g.pushMatrix();
    g.translate(pose.pos());
    g.rotate(pose.quat());
    g.color(color);
    if (id % 5 == 0) {
      g.draw(fishMeshS);
    } else if (id % 2 == 0) {
      g.draw(fishMeshM);
    } else {
      g.draw(fishMeshL);
    }
    g.popMatrix();
  }

  Vec3f separate() {
    int count = 0;
    Vec3f steer;

    for (auto other : *fishes) {
      // this difference is a vector from b to a, put this force on a, so
      // push away.
      Vec3f difference = (pose.pos() - other.pose.pos());
      float d = difference.mag();
      // if agents is getting closer, push away
      if (d > 0 && d < 6 * sphereRadius) {
        steer += difference.normalize() / d;
        count++;
      }
    }
    if (count > 0) {
      steer = steer / count;
    }
    if (steer.mag() > 0) {
      steer = steer.normalize() * maxSpeed;
      steer -= velocity;
    }
    return steer;
  }

  Vec3f align() {
    int count = 0;
    Vec3f steer;
    Vec3f sum;

    for (auto other : *fishes) {
      Vec3f difference = (pose.pos() - other.pose.pos());
      float d = difference.mag();
      if (d > 0 && d < 60 * sphereRadius) {
        sum += other.acceleration;
        count++;
      }
    }
    if (count > 0) {
      sum = sum / count;
      sum.normalize(maxSpeed);
      steer = sum - velocity;
      return steer;
    } else {
      return Vec3f(0, 0, 0);
    }
  }

  Vec3f cohesion() {
    int count = 0;
    Vec3f sum;
    // Quat<float> sumQuat;

    for (auto other : *fishes) {
      Vec3f difference = (pose.pos() - other.pose.pos());
      float d = difference.mag();
      if (d > 0 && d < 60 * sphereRadius) {
        sum += other.pose.pos();
        // sumQuat += other.pose.quat();
        count++;
      }
    }
    if (count > 0) {
      sum = sum / count;
      // sumQuat = sumQuat / count;
      // pose.lerp(sumQuat, 0.1);
      // pose.slerpTo(sumQuat, 0.1);
      sum.normalize(maxSpeed);
      return seek(sum);
    } else {
      return Vec3f(0, 0, 0);
    }
  }

  Vec3f seek(Vec3f target) {
    Vec3f desired = target - pose.pos();
    Vec3f steer = desired.normalize(maxSpeed);
    steer -= velocity;
    return steer;
  }

  void eaten() { alive = false; }

  void seekTarget(Vec3f target) {
    Vec3f steer = seek(target).normalize();
    steer = steer * maxSpeed;
    applyForce(steer);
  }

  void runAway(Vec3f target) {
    Vec3f desired = target - pose.pos();
    Vec3f steer = -(desired.normalize() * maxSpeed);
    steer = (steer - velocity) * 20.0f;
    applyForce(steer);
  }

  Vec3f stayInCircle() {
    // needs a way to normalize the .mag()
    float d = pose.pos().mag();
    if (d > OUT_BOUND) {
      return seek(Vec3f(0, 0, 0));
    } else
      // return seek(Vec3f(pose.pos().x + 10 * velocity.x,
      //                   rnd::uniform(initRadius),
      //                   pose.pos().z + 10 * velocity.z));
      return seek(rnd::ball<Vec3f>() * 2 * initRadius);
    // return seek(Vec3f(velocity.x, rnd::uniformS(initRadius * 2),
    // velocity.z));
  }

  void applyForce(Vec3f force) { acceleration += force; }
};

// UserFish
struct UserFish {
  Vec3f velocity, acceleration, lastPos;
  Nav nav;
  Color color;
  Mesh tentacles;
  bool autoMode;
  // Quatf targetQuat;
  // Vec3f targetPos;

  UserFish() {
    nav.pos() = Vec3f(0, 0, 0) * initRadius * 2;
    velocity = Vec3f(0, 0, 0);
    velocity = Vec3f(0, 0, 0);
    color = RGB(1);

    tentacles.primitive(Graphics::LINES);
    for (int i = 0; i < TAIL_LENGTH; i++) {
      tentacles.vertex(Vec3f());
      tentacles.color(RGB(1));
    }
    // id = 0;
  }

  void draw(Graphics &g) {
    g.pushMatrix();
    g.translate(nav.pos());
    g.rotate(nav.quat());
    g.color(color);
    g.draw(userFishMesh);
    g.popMatrix();
    g.draw(tentacles);
  }

  void update() {
    velocity += acceleration * timeStep;
    nav.pos() += velocity * timeStep;
    acceleration.zero();  // reset acceleration after each update
  }

  void findNewTarget(vector<Fish> fishes) {
    float nearestFish = 10000.f;
    for (auto fish : fishes) {
      float d = (nav.pos() - fish.pose.pos()).mag();
      if (d > 300 && d < nearestFish) {
        nearestFish = d;
        // targetQuat = fish.pose.quat();
        // targetPos = fish.pose.pos();
        targetFishID = fish.id;
      }
    }
  }

  void seekTarget(Vec3f targetPos) {
    Vec3f desired = targetPos - nav.pos();
    Vec3f steer = desired.normalize() * maxAcceleration;
    // Vec3f steer = desired.normalize() * maxSpeed;
    steer = steer - velocity;
    applyForce(steer);
  }

  void applyForce(Vec3f force) { acceleration += force; }
};

// Ghost net
struct GhostNet {
  Mesh ghostNetMesh, boundingBoxMesh;
  Vec3f velocity, acceleration, center;
  Nav nav;
  Color color;
  double timePast;
  int total;

  GhostNet() {
    nav.pos() =
        Vec3f(rnd::uniform(100), rnd::uniform(400, 100), rnd::uniform(100));
    nav.quat().set(float(rnd::uniform()), float(rnd::uniform()),
                   float(rnd::uniform()), float(rnd::uniform()));
    velocity = Vec3f(0, 0, 0);
    color = RGB(0.95f);

    // generate the shape
    addSurface(ghostNetMesh, rnd::uniform(25, 15), rnd::uniform(50, 25),
               rnd::uniform(50, 30), rnd::uniform(100, 50));

    // randomize the vertices
    for (int i = 0; i < ghostNetMesh.vertices().size(); i++) {
      ghostNetMesh.vertices()[i] += rnd::uniformS(5);
    }

    ghostNetMesh.primitive(Graphics::LINE_LOOP);
    ghostNetMesh.generateNormals();

    total = ghostNetMesh.vertices().size();
  }

  void wiggle(double dt) {
    float topY = -FLT_MAX, bottomY = FLT_MAX, leftX = -FLT_MAX,
          rightX = FLT_MAX, nearZ = -FLT_MAX, farZ = FLT_MAX;
    float record = 0.f;
    // add random offset to vertices to make them wiggle and deform
    for (int i = 0; i < total - 1; i++) {
      Vec3f *VertPointer;
      VertPointer = &(ghostNetMesh.vertices()[i]);
      Vec3f VertV, VertA, steer;

      // calculate ghost net's top vertices
      // if (*VertPointer.y > topY) topY = *VertPointer.y;
      // if (*VertPointer.y < bottomY) bottomY = *VertPointer.y;
      // if (*VertPointer.x > leftX) leftX = *VertPointer.x;
      // if (*VertPointer.x < rightX) rightX = *VertPointer.x;
      // if (*VertPointer.z > nearZ) nearZ = *VertPointer.z;
      // if (*VertPointer.z < farZ) farZ = *VertPointer.z;

      Vec3f diff = ghostNetMesh.vertices()[i] - ghostNetMesh.vertices()[i + 1];
      Vec3f midd =
          (ghostNetMesh.vertices()[i] + ghostNetMesh.vertices()[i + 1]) / 2;
      float d = diff.mag();
      if (d > 0 && d < 5) {
        diff.normalize(maxSpeed);
        steer = diff - VertV;
      } else {
        midd -= midd - ghostNetMesh.vertices()[i];
        midd.normalize(maxSpeed);
        steer = midd;
      }
      VertA += steer;

      VertV += VertA * timeStep;
      *VertPointer += VertV * timeStep;
    }
    ghostNetMesh.generateNormals();
  }

  void flowInSea(vector<Fish> fishes) {
    int count = 0;
    Vec3f steer;
    Vec3f sum;

    for (auto fish : fishes) {
      Vec3f diff = (nav.pos() - fish.pose.pos()).mag();
      if (diff > 0 && diff < 100 * sphereRadius) {
        sum += fish.acceleration;
        count++;
      }
    }
    if (count > 0) {
      sum = sum / count;
      sum.normalize(maxSpeed / 2);
      steer = sum - velocity;
      applyForce(steer);
    }
    if (nav.pos().y > 0) {
      applyForce(Vec3f(0, -2, 0));
    } else {
      applyForce(Vec3f(0, 1, 0));
    }
  }

  void applyForce(Vec3f force) { acceleration += force; }

  void update() {
    velocity += acceleration * timeStep;
    nav.pos() += velocity * timeStep;
    acceleration.zero();  // reset acceleration after each update
  }

  void draw(Graphics &g) {
    g.pushMatrix();
    g.translate(nav.pos());
    g.rotate(nav.quat());
    g.color(color);
    g.draw(ghostNetMesh);
    g.popMatrix();
    // g.draw(tentacles);
  }
};

string fullPathOrDie(string fileName, string whereToLook = ".") {
  SearchPaths searchPaths;
  searchPaths.addSearchPath(whereToLook);
  string filePath = searchPaths.find(fileName).filepath();
  assert(filePath != "");
  return filePath;
}

// MyApp
/////////////////////////////
struct MyApp : App {
  // for text and image
  Mesh bgMesh;
  Texture bgTexture;
  Texture planktonTexture;
  Texture spriteTex;

  // general environment setting
  Material material;
  Light light;
  bool simulate = true;

  // creating the real particleList, it's now empty
  vector<Fish> fishZeroList;
  vector<Plankton> planktonList;
  UserFish userFishZero;
  GhostNet ghostNet0;

  // for sound
  gam::SineD<> sined;
  gam::Accum<> timer;

  MyApp() : spriteTex(16, 16, Graphics::LUMINANCE, Graphics::FLOAT) {
    light.pos(0, 0, 0);   // place the light
    nav().pos(0, 0, 50);  // place the viewer
    background(Color(0.1));

    // set near/far clip
    lens().near(0.1);
    lens().far(1000);  // increase far clip to 1000 GL Units

    // generate sphere with texture coordinates
    {
      addSphereWithTexcoords(bgMesh);

      // load image into texture print out error and exit if failure
      Image image;
      SearchPaths searchPaths;
      searchPaths.addSearchPath(".");
      string filename =
          searchPaths.find("DSC06307_Panorama_dark.jpg").filepath();
      if (image.load(filename)) {
        cout << "Read image from " << filename << endl;
      } else {
        cout << "Failed to read image from " << filename << "!!!" << endl;
        exit(-1);
      }
      bgTexture.allocate(image.array());
    }

    // add vertices to empty Meshes
    {
      addCone(userFishMesh, sphereRadius * 3, Vec3f(0, 0, sphereRadius * 12),
              16, 1);
      addCone(fishMeshS, sphereRadius * 0.5, Vec3f(0, 0, sphereRadius * 1.5),
              16, 1);
      addCone(fishMeshM, sphereRadius * 1, Vec3f(0, 0, sphereRadius * 3), 16,
              1);
      addCone(fishMeshL, sphereRadius * 1.5, Vec3f(0, 0, sphereRadius * 4.5),
              16, 1);
      userFishMesh.generateNormals();
      fishMeshS.generateNormals();
      fishMeshM.generateNormals();
      fishMeshL.generateNormals();
    }

    // pushing every Particle instance into the actual list
    for (int i = 0; i < fishCount; i++) {
      Fish newFish(&fishZeroList, i);
      fishZeroList.push_back(newFish);
    }

    // plankton
    for (int i = 0; i < fishCount * 2; i++) {
      Plankton newPlankton(&planktonList, i);
      planktonList.push_back(newPlankton);
    }
    // Create a Gaussian "bump" function to use for the sprite
    int Nx = spriteTex.width();
    int Ny = spriteTex.height();
    float *pixels = spriteTex.data<float>();

    for (int j = 0; j < Ny; ++j) {
      float y = float(j) / (Ny - 1) * 2 - 1;
      for (int i = 0; i < Nx; ++i) {
        float x = float(i) / (Nx - 1) * 2 - 1;
        float m = exp(-3 * (x * x + y * y));
        pixels[j * Nx + i] = m;
      }
    }

    initWindow();
    initAudio();
    userFishZero.findNewTarget(fishZeroList);
  }

  void onAnimate(double dt) {
    // reset two variable for sound
    nearbyFish = 0;
    myFrameRate = 1 / dt;
    cout << "Current Frame Rate = " << myFrameRate << endl;

    if (!simulate)
      // skip the rest of this function
      return;

    // light position
    light.pos(userFishZero.nav.pos());

    // userFish animation
    userFishZero.update();
    // if (userFishZero.autoMode) {
    userFishZero.seekTarget(fishZeroList[targetFishID].pose.pos());
    userFishZero.nav.faceToward(fishZeroList[targetFishID].pose.pos(), 0.05);
    if (fishZeroList[targetFishID].alive == false) {
      userFishZero.findNewTarget(fishZeroList);
    }

    // fish animation
    for (int i = 0; i < fishZeroList.size(); ++i) {
      Vec3d *mePosPointer;
      mePosPointer = &(fishZeroList[i].pose.pos());

      Fish me = fishZeroList[i];

      me.update();

      // get distance between user fish
      Vec3f diff_predator = me.pose.pos() - userFishZero.nav.pos();
      float d2 = diff_predator.mag();

      // run away from predator
      if (d2 < 60 * sphereRadius) {
        if (i != targetFishID) {
          me.runAway(userFishZero.nav.pos());
        }
      }
      // mark nearby fish dead
      if (d2 < 3 * sphereRadius) {
        me.eaten();
      }

      if (d2 < 30 * sphereRadius) {
        nearbyFish += 1;
      }

      // ghost net catching fish
      for (int i = 0; i < ghostNet0.ghostNetMesh.vertices().size(); i++) {
        Vec3f &position = ghostNet0.ghostNetMesh.vertices()[i];
        float d = (me.pose.pos() - position).mag();
        if (d < 3 * sphereRadius) {
          *mePosPointer = Vec3d(position.x, position.y, position.z);
        }
      }

      ///////// this is causing a bug
      me.targetP_diff = 10000.f;  // reset target diff to 10000;
      for (int ii = 0; ii < planktonList.size(); ii++) {
        float p_diff = (me.pose.pos() - planktonList[ii].position).mag();
        if (p_diff < me.targetP_diff) {
          me.targetP_diff = p_diff;
          me.targetID = ii;
        }
      }
      if (me.targetP_diff > 2 * sphereRadius) {
        me.seekTarget(planktonList[me.targetID].position);
        me.pose.faceToward(planktonList[me.targetID].position, 0.01);
      } else {
        planktonList[me.targetID].eaten();
      }

      fishZeroList[i] = me;
    }

    // plankton animation
    for (int i = 0; i < planktonList.size(); ++i) {
      planktonList[i].update();
    }

    // ghost net animation
    ghostNet0.wiggle(dt);
    ghostNet0.flowInSea(fishZeroList);
    ghostNet0.update();
    ghostNet0.nav.quat().slerpTo(userFishZero.nav.quat(), 0.001);

    // for each fish...
    //   find the closest vertex in the ghostnet mesh
    //   if it's within N units, 'catch" the fish
    //   catching the fish means applying a force
    for (int i = 0; i < ghostNet0.ghostNetMesh.vertices().size(); i++) {
      Vec3f &position = ghostNet0.ghostNetMesh.vertices()[i];
    }

    // how close is the target to viewer
    Vec3f diff_nav = nav().pos() - userFishZero.nav.pos();
    targetToNav = diff_nav.mag();  // it ranges from 50 - 100
    timer.freq(nearbyFish);
  }

  void onDraw(Graphics &g, const Viewpoint &v) {
    // Enable blending to hide texture edges
    // causing very slow rendering
    g.blendAdd();

    // draw background textured sphere centered at nav
    // turn off lighting
    g.lighting(false);
    // disable depth buffer, so that background will be drawn over
    g.depthMask(false);

    g.pushMatrix();
    g.translate(nav().pos());
    g.rotate(180, 0, 0, 1);
    bgTexture.bind();
    g.color(1, 1, 1);
    g.draw(bgMesh);
    bgTexture.unbind();
    g.popMatrix();

    g.depthMask(true);  // turn depth mask back on

    // draw original scene here
    light();  // enable light
    material();
    g.pushMatrix();
    g.scale(scaleFactor);
    for (auto fish : fishZeroList) {
      if (fish.alive == true) {
        fish.draw(g);
      }
    }
    g.popMatrix();

    g.pushMatrix();
    userFishZero.draw(g);
    ghostNet0.draw(g);
    g.popMatrix();

    // draw plankton with point sprite
    // g.pushMatrix();
    glEnable(GL_POINT_SPRITE);
    glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE);

    // Setting the point size sets the sprite size
    g.pointSize(10);

    // We must bind our sprite texture before drawing the points
    spriteTex.bind();
    for (auto p : planktonList) {
      if (p.alive == true) {
        p.draw(g);
      }
    }
    spriteTex.unbind();
    glDisable(GL_POINT_SPRITE);
    // g.popMatrix();
  }

  void onKeyDown(const ViewpointWindow &, const Keyboard &k) {
    float targetPosition = 10;
    switch (k.key()) {
      default:
      case 'u':
        userFishZero.nav.pos().y += targetPosition;
        break;
      case 'j':
        userFishZero.nav.pos().y -= targetPosition;
        break;
      case 'h':
        userFishZero.nav.pos().x -= targetPosition;
        break;
      case 'k':
        userFishZero.nav.pos().x += targetPosition;
        break;
      case '1':
        // reverse time
        timeStep *= -1;
        break;
      case '2':
        // speed up time
        if (timeStep < 1) timeStep *= 2;
        break;
      case '3':
        // slow down time
        if (timeStep > 0.0005) timeStep /= 2;
        break;
      case '4':
        // pause the simulation
        //   simulate = !simulate;
        break;
      case '5':
        // change auto/manual control
        if (userFishZero.autoMode)
          userFishZero.autoMode = false;
        else
          userFishZero.autoMode = true;
        break;
    }
  }
  virtual void onMouseDown(const ViewpointWindow &w, const Mouse &m) {
    // normalize mouse position from -1.0 to 1.0
    float x = float(m.x()) / w.width() * 2.f - 1.f;
    float y = (float(m.y()) / w.height() * 2.f - 1.f) * -1.f;

    // move light with mouse
    light.pos(Vec3f(x, y, 1.f) * 10.f);
  }

  // virtual void onSound(AudioIOData &io) {
  //   gam::Sync::master().spu(audioIO().fps());
  //   while (io()) {
  //     if (timer()) {
  //       // sined.set(rnd::uniform(220.0f, 880.0f), 0.5f, 1.0f);
  //       sined.set(1000.0f - targetToNav * 6, 0.5f, 1.0f);
  //     }
  //     float s = sined();
  //     io.out(0) = s;
  //     io.out(1) = s;
  //   }
  // }
};

int main() {
  MyApp app;
  app.start();
}