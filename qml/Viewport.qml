import QtQuick
import QtQuick3D
import SkinStudio

Item {
    id: vp
    property bool pickEnabled: true
    property bool inputEnabled: true
    readonly property real h: AppController.modelHeight

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#1a2130" }
            GradientStop { position: 0.75; color: "#0f131b" }
            GradientStop { position: 1.0; color: "#0b0e14" }
        }
    }

    // camera: orbit around the model with clamped distance / pitch / pan (the model can never be lost)
    property real yaw: 0
    property real pitch: -12
    property real dist: h * 2.1
    property real panX: 0
    property real panY: 0
    readonly property real minDist: h * 0.9
    readonly property real maxDist: h * 5
    function resetView() { yaw = 0; pitch = -12; dist = h * 2.1; panX = 0; panY = 0 }
    function zoomBy(f) { dist = Math.max(minDist, Math.min(maxDist, dist * f)) }
    onHChanged: resetView()

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
            x: vp.panX; y: vp.h * 0.5 + vp.panY
            eulerRotation.y: vp.yaw
            eulerRotation.x: vp.pitch
            PerspectiveCamera {
                id: cam
                z: vp.dist
                fieldOfView: 40
                clipNear: Math.max(1, vp.dist * 0.02); clipFar: vp.dist + vp.h * 10
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

    // input layer (below the overlay controls, so their clicks are never stolen)
    Item {
        id: input
        enabled: vp.inputEnabled
        anchors.fill: parent
        DragHandler {
            grabPermissions: PointerHandler.TakeOverForbidden
            id: rotDrag
            target: null
            acceptedButtons: Qt.LeftButton
            property real sy; property real sp
            onActiveChanged: if (active) { sy = vp.yaw; sp = vp.pitch }
            onTranslationChanged: if (active) {
                vp.yaw = sy - translation.x * 0.4
                vp.pitch = Math.max(-75, Math.min(8, sp - translation.y * 0.3))
            }
        }
        DragHandler {
            grabPermissions: PointerHandler.TakeOverForbidden
            target: null
            acceptedButtons: Qt.RightButton | Qt.MiddleButton
            property real sx; property real sy
            onActiveChanged: if (active) { sx = vp.panX; sy = vp.panY }
            onTranslationChanged: if (active) {
                const k = vp.dist / Math.max(1, vp.height) * 0.75
                const lim = vp.h * 0.6
                vp.panX = Math.max(-lim, Math.min(lim, sx - translation.x * k))
                vp.panY = Math.max(-lim, Math.min(lim, sy + translation.y * k))
            }
        }
        WheelHandler {
            grabPermissions: PointerHandler.TakeOverForbidden
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onWheel: (ev) => { vp.zoomBy(Math.pow(0.999, ev.angleDelta.y + ev.pixelDelta.y)); ev.accepted = true }
        }
        PinchHandler { target: null; onScaleChanged: (d) => vp.zoomBy(1 / d) }

        TapHandler {
            grabPermissions: PointerHandler.TakeOverForbidden
            acceptedButtons: Qt.LeftButton
            gesturePolicy: TapHandler.DragThreshold
            onDoubleTapped: vp.resetView()
            onTapped: (ev) => {
                if (!vp.pickEnabled) return
                const r = view.pick(ev.position.x, ev.position.y)
                if (r.objectHit && r.objectHit.partIndex !== undefined)
                    AppController.pickUv(r.objectHit.partIndex, r.uvPosition.x, r.uvPosition.y)
                else AppController.selectedRegion = -1
            }
        }
    }

    Column {
        anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 14
        spacing: 4
        Rectangle {
            anchors.right: parent.right
            width: resetTxt.implicitWidth + 24; height: 28; radius: 8
            color: resetMa.containsMouse ? Theme.panel2 : Qt.rgba(0, 0, 0, 0.25)
            border.color: Theme.line
            Text { id: resetTxt; anchors.centerIn: parent; text: "↺  Görünümü sıfırla"; color: Theme.text; font.pixelSize: 12; font.weight: Font.DemiBold }
            MouseArea {
                id: resetMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                onClicked: vp.resetView()
            }
        }
        Text { anchors.right: parent.right; text: "Sürükle: döndür · Tekerlek: yakınlaş · Sağ tık sürükle: kaydır · Çift tık: sıfırla"; color: Theme.faint; font.pixelSize: 11 }
        Text { anchors.right: parent.right; visible: vp.pickEnabled; text: "Rengini değiştirmek istediğin yere tıkla"; color: Theme.muted; font.pixelSize: 11 }
    }
    Text {
        anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 14
        text: "Önizleme oyundaki gibi tek yüzlü çizilir; ışık oyundan farklıdır."
        color: Theme.faint; font.pixelSize: 11
    }
}
