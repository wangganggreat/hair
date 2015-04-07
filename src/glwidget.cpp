#include "glwidget.h"
#include "resourceloader.h"
#include "errorchecker.h"
#include "hairCommon.h"
#include <ctime>
#include <chrono>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include <QLineEdit>
#include <QGraphicsView>
#include <QBrush>

#include "hair.h"
#include "hairobject.h"
#include "simulation.h"
#include "objmesh.h"

#include "mainwindow.h"
#include "ui_mainwindow.h"

#define _USE_MESH_ true

GLWidget::GLWidget(QGLFormat format, QWidget *parent)
    : QGLWidget(format, parent), m_timer(this), m_targetFPS(60.f), m_increment(0), m_hairDensity(40)
{
    // Set up 60 FPS draw loop.
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(tick()));
    m_timer.start(1000.0f / m_targetFPS);
    
    m_ui = NULL;
    m_mesh = NULL;
    m_hairObject = NULL;
    m_testSimulation = NULL;
}

GLWidget::~GLWidget()
{
#if _USE_MESH_
    safeDelete(m_mesh);
#endif
    safeDelete(m_testSimulation);
    safeDelete(m_hairObject);
}

void GLWidget::initializeGL()
{
    ResourceLoader::initializeGlew();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    
    m_meshProgramID = ResourceLoader::createBasicShaderProgram(
                ":/shaders/basic.vert", ":/shaders/basic.frag");
    
    m_hairProgram.create();
    
    initSimulation();
    
    ErrorChecker::printGLErrors("end of initializeGL");
}


void GLWidget::initSimulation(){
    delete m_mesh;
    delete m_testSimulation;
    HairObject *_oldHairObject = m_hairObject;
    
    m_testSimulation = new Simulation();
    
#if _USE_MESH_
    m_mesh = new ObjMesh();
    m_mesh->init(":/models/sphere.obj");
    m_hairObject = new HairObject(
                m_mesh, m_hairDensity, ":/images/lower.png", m_testSimulation, m_hairObject);
#else
    if (_oldHairObject != NULL){
        m_hairObject = new HairObject(_oldHairObject, m_testSimulation);
    } else {
        m_hairObject = new HairObject(1, m_testSimulation);
    }
#endif
    
    delete _oldHairObject;
    
    syncUI();
}


void GLWidget::paintGL()
{
    ErrorChecker::printGLErrors("start of paintGL");
        
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    float time = m_increment++ / (float) m_targetFPS;      // Time in seconds.
    
    m_testSimulation->update(time);
    m_hairObject->update(time);
    
    m_hairProgram.bind();
    m_hairProgram.uniforms.projection = glm::perspective(0.8f, (float)width()/height(), 0.1f, 100.f);
    m_hairProgram.uniforms.view = glm::lookAt(
                glm::vec3(0.f, 0.f, 6.f),  // eye
                glm::vec3(0.f, 0.f, 0.f),  // center
                glm::vec3(0.f, 1.f, 0.f)); // up
    m_hairProgram.setGlobalUniforms();
    
    m_hairProgram.uniforms.model = glm::mat4(1.f);
    m_hairObject->paint(m_hairProgram);
    m_hairProgram.unbind();
    
#if _USE_MESH_
    glUseProgram(m_meshProgramID);
    glUniformMatrix4fv(glGetUniformLocation(m_meshProgramID, "projection"), 1, GL_FALSE,
                       glm::value_ptr(m_hairProgram.uniforms.projection));
    glUniformMatrix4fv(glGetUniformLocation(m_meshProgramID, "view"), 1, GL_FALSE,
                       glm::value_ptr(m_hairProgram.uniforms.view));
    glUniformMatrix4fv(glGetUniformLocation(m_meshProgramID, "model"), 1, GL_FALSE,
                       glm::value_ptr(m_hairProgram.uniforms.model));
    m_mesh->draw();
    glUseProgram(0);
#endif

    int updateFrequency = 10;
    if (m_increment % updateFrequency == 0) {
        // Update FPS label.
        if (m_increment > 0) {
            int fps = updateFrequency * 1000.0 / m_clock.elapsed();
            m_ui->fpsLabel->setText(QString::number(fps, 'f', 1) + " FPS");
        }

        // Update stats label.
        int numGuideHairs = m_hairObject->m_guideHairs.size();
        int numGroupHairs = m_hairObject->m_numGroupHairs;
        int numGuideVertices = m_hairObject->m_guideHairs[0]->m_vertices.size();
        int numSplineVertices = m_hairObject->m_numSplineVertices;
        m_ui->statsLabel->setText(
                    QString::number(numGuideHairs) + " guide hairs\n" +
                    QString::number(numGuideHairs * numGroupHairs) + " rendered hairs\n" +
                    QString::number(numGuideHairs * numGuideVertices) + " simulated vertices\n" +
                    QString::number(numGuideHairs * numGroupHairs * numSplineVertices * 4) + " rendered vertices");

        m_clock.restart();
    }
}

void GLWidget::setUI(Ui::MainWindow *ui)
{
    m_ui = ui;
    
    // hairs per patch
    QSlider *slider = m_ui->sliderHairsPerPatch;
    connect(slider, SIGNAL(valueChanged(int)), this, SLOT(setHairsPerPatch(int)));
    
    // spline vertices
    slider = m_ui->sliderSplineVertices;
    connect(slider, SIGNAL(valueChanged(int)), this, SLOT(setSplineVertices(int)));
    
    // rgb
    slider = m_ui->sliderHairColorR;
    connect(slider, SIGNAL(valueChanged(int)), this, SLOT(setHairColorR(int)));
    slider = m_ui->sliderHairColorG;
    connect(slider, SIGNAL(valueChanged(int)), this, SLOT(setHairColorG(int)));
    slider = m_ui->sliderHairColorB;
    connect(slider, SIGNAL(valueChanged(int)), this, SLOT(setHairColorB(int)));
}
 
void GLWidget::syncUI()
{
    // hairs per patch
    m_ui->sliderHairsPerPatch->setValue(m_hairObject->m_numGroupHairs);
    m_ui->inputHairsPerPatch->setText(QString::number(m_hairObject->m_numGroupHairs));
    
    // spline vertices
    m_ui->sliderSplineVertices->setValue(m_hairObject->m_numSplineVertices);
    m_ui->inputSplineVertices->setText(QString::number(m_hairObject->m_numSplineVertices));
    
    // rgb
    m_ui->sliderHairColorR->setValue(m_hairObject->m_color.x*2550);
    m_ui->sliderHairColorG->setValue(m_hairObject->m_color.y*2550);
    m_ui->sliderHairColorB->setValue(m_hairObject->m_color.z*2550);
    m_ui->inputHairColorR->setText(QString::number(m_hairObject->m_color.x, 'g', 2));
    m_ui->inputHairColorG->setText(QString::number(m_hairObject->m_color.y, 'g', 2));
    m_ui->inputHairColorB->setText(QString::number(m_hairObject->m_color.z, 'g', 2));
    
    // reset button
    QPushButton *button = m_ui->buttonResetSim;
    connect(button, SIGNAL(pressed()), this, SLOT(resetSimulation()));
}

void GLWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

/** Repaints the canvas. Called 60 times per second. */
void GLWidget::tick()
{
    update();
}

void GLWidget::resetSimulation(){
    initSimulation();
}

void GLWidget::setHairsPerPatch(int numHairs){
    m_hairObject->m_numGroupHairs = numHairs;
    m_ui->inputHairsPerPatch->setText(QString::number(numHairs));
}

void GLWidget::setSplineVertices(int numVertices){
    m_hairObject->m_numSplineVertices = numVertices;
    m_ui->inputSplineVertices->setText(QString::number(numVertices));
}

void GLWidget::setHairColorR(int value){
    m_hairObject->m_color.x = value/2550.;
    m_ui->inputHairColorR->setText(QString::number(m_hairObject->m_color.x, 'g', 2));
}
void GLWidget::setHairColorG(int value){
    m_hairObject->m_color.y = value/2550.;
    m_ui->inputHairColorG->setText(QString::number(m_hairObject->m_color.y, 'g', 2));
}
void GLWidget::setHairColorB(int value){
    m_hairObject->m_color.z = value/2550.;
    m_ui->inputHairColorB->setText(QString::number(m_hairObject->m_color.z, 'g', 2));
}
