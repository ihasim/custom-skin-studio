#pragma once
// One textured part of the skinned model (the submeshes that share a texture), skinned on the CPU every frame.
// League is left-handed: X is mirrored and the triangle winding reversed so back-face culling matches the game.
#include <QQuick3DGeometry>
#include <QQuick3DTextureData>
#include <QtQml/qqmlregistration.h>
#include <vector>

class SkinGeometry : public QQuick3DGeometry {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int part READ part WRITE setPart NOTIFY partChanged)
public:
    explicit SkinGeometry(QQuick3DObject *parent = nullptr);
    int part() const { return part_; }
    void setPart(int p);
signals:
    void partChanged();
private:
    void rebuild();
    void pose();
    int part_ = -1;
    std::vector<uint32_t> verts_;   // global vertex index per local vertex
    QByteArray buf_;
};

class SkinTexture : public QQuick3DTextureData {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int part READ part WRITE setPart NOTIFY partChanged)
public:
    explicit SkinTexture(QQuick3DObject *parent = nullptr);
    int part() const { return part_; }
    void setPart(int p);
signals:
    void partChanged();
private:
    void refresh();
    int part_ = -1;
};
