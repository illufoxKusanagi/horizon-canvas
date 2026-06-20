#ifndef GENERATION_SETTINGS_H
#define GENERATION_SETTINGS_H

#include <QString>

struct GenerationSettings {
    int maxThreads = 0; // 0 means auto
    int randomSamples = 1000;
    int mutateSamples = 200;
    int maxShapes = 5000;
    int maxResolution = 1400;
    int saveInterval = 10;
    QString customCheckpoints = "";
    bool useOpenCL = true;
    QString preprocessMode = "none";
    QString profileName = "custom";
};

#endif // GENERATION_SETTINGS_H
