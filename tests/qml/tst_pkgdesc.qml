import QtQuick 2.15
import QtTest 1.3

TestCase {
    name: "PkgDescBehavior"

    function createDescriptor() {
        var component = Qt.createComponent("../../pkgdesc.qml");
        compare(component.status, Component.Ready, component.errorString());
        var object = component.createObject(null);
        verify(object !== null);
        return object;
    }

    function fakeFwParams(hwType) {
        return {
            hwTypeStr: function() {
                return hwType;
            }
        };
    }

    function test_vesc_hardware_is_compatible_case_insensitively() {
        var descriptor = createDescriptor();

        verify(descriptor.isCompatible(fakeFwParams("VESC")));
        verify(descriptor.isCompatible(fakeFwParams("vesc")));
        verify(descriptor.isCompatible(fakeFwParams("VeSc")));

        descriptor.destroy();
    }

    function test_non_vesc_hardware_is_rejected() {
        var descriptor = createDescriptor();

        verify(!descriptor.isCompatible(fakeFwParams("unity")));
        verify(!descriptor.isCompatible(fakeFwParams("custom-board")));
        verify(!descriptor.isCompatible(fakeFwParams("")));

        descriptor.destroy();
    }
}
