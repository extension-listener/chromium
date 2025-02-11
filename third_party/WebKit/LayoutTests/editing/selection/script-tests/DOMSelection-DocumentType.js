// TODO(yosin): We should move this script to DOMSelection-DocumentType.html and
// use w3c test harness.
description("Test to check if setBaseAndExtent guard node with null owner document (Bug 31680)");

var sel = window.getSelection();
var docType = document.implementation.createDocumentType('c', null, null);

sel.setBaseAndExtent(docType, 0, null, 0);
shouldBeNull("sel.anchorNode");

sel.setBaseAndExtent(null, 0, docType, 0);
shouldBeNull("sel.anchorNode");

sel.collapse(docType);
shouldBeNull("sel.anchorNode");

sel.selectAllChildren(docType);
shouldBeNull("sel.anchorNode");

shouldThrow("sel.extend(docType, 0)");
shouldBeNull("sel.anchorNode");

sel.containsNode(docType);
shouldBeNull("sel.anchorNode");

shouldBeFalse("sel.containsNode(docType)");

var successfullyParsed = true;
