import QtQuick
import QtQuick3D
import QtQuick3D.Helpers
import SkinStudio

Item {
    id: vp
    property bool pickEnabled: true
    readonly property real h: AppController.modelHeight

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#1a2130" }
            GradientStop { position: 0.75; color: "#0f131b" }
            GradientStop { position: 1.0; color: "#0b0e14" }
        }
    }

    View3D {
        id: view
        anchors.fill: parent
        environment: SceneEnvironment {
            backgroundMode: SceneEnvironment.Transparent
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High
        }

        Node {
            id: origin
            y: vp.h * 0.5
            eulerRotation.x: -12
            PerspectiveCamera {
                id: cam
                z: vp.h * 2.1
                fieldOfView: 40
                clipNear: 5; clipFar: 10000
            }
        }

        DirectionalLight { eulerRotation.x: -35; eulerRotation.y: 25; brightness: 0.6 }

        // floor disc
        Model {
            source: "#Cylinder"
            y: -1
            scale: Qt.vector3d(vp.h * 0.011, 0.01, vp.h * 0.011)
            materials: PrincipledMaterial { baseColor: "#161c27"; lighting: PrincipledMaterial.NoLighting }
        }

        Node {
            id: model
            Repeater3D {
                model: AppController.partCount
                delegate: Model {
                    required property int index
                    property int partIndex: index
                    pickable: true
                    geometry: SkinGeometry { part: index }
                    materials: PrincipledMaterial {
                        lighting: PrincipledMaterial.NoLighting
                        cullMode: Material.BackFaceCulling
                        baseColorMap: Texture {
                            textureData: SkinTexture { part: index }
                            generateMipmaps: true
                            mipFilter: Texture.Linear
                        }
                    }
                }
            }
        }
    }

    OrbitCameraController {
        id: orbit
        anchors.fill: parent
        origin: origin
        camera: cam
        panEnabled: true
    }

    TapHandler {
        enabled: vp.pickEnabled
        acceptedButtons: Qt.LeftButton
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: (ev) => {
            const r = view.pick(ev.position.x, ev.position.y)
            if (r.objectHit && r.objectHit.partIndex !== undefined)
                AppController.pickUv(r.objectHit.partIndex, r.uvPosition.x, r.uvPosition.y)
            else AppController.selectedRegion = -1
        }
    }

    Column {
        anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 14
        spacing: 4
        Text { anchors.right: parent.right; text: "Sürükle: döndür · Kaydır: yakınlaş · Sağ tık sürükle: kaydır"; color: Theme.faint; font.pixelSize: 11 }
        Text { anchors.right: parent.right; visible: vp.pickEnabled; text: "Rengini değiştirmek istediğin yere tıkla"; color: Theme.muted; font.pixelSize: 11 }
    }
    Text {
        anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 14
        text: "Önizleme oyundaki gibi tek yüzlü çizilir; ışık oyundan farklıdır."
        color: Theme.faint; font.pixelSize: 11
    }
}
