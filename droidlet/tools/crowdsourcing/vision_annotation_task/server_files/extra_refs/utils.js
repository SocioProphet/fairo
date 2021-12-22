/*
   Copyright (c) Facebook, Inc. and its affiliates.

 * utils.js contains a variety of click handlers and timing analytics for vision_annotation_task
 */

// Remove the stylesheet inherited from Mephisto by default
$('link[rel=stylesheet][href~="https://cdn.jsdelivr.net/npm/bulma@0.8.2/css/bulma.min.css"]').remove();

var blockMarked = false;
var annotationFinished = false;
var ratingComplete = false;
var feedbackGiven = false;
var clickedElements = new Array();

// recordClick logs user actions as well as messages from the dashboard to the Mephisto form
function recordClick(ele) {
  if (ele === "block_marked") {  // Check to allow submission if all qualifications are met
    blockMarked = true;
    checkSubmitDisplay();
  }
  if (ele === "annotation_finished") {  // Check to allow submission if all qualifications are met
    annotationFinished = true;
    checkSubmitDisplay();
  }
  clickedElements.push({id: ele, timestamp: Date.now()});
  document.getElementById("clickedElements").value = JSON.stringify(clickedElements);
  console.log("Clicked elements array: " + JSON.stringify(clickedElements));
}
recordClick("start");

// Log data from postMessage, and record it if it seems to come from the dashboard
var data;
if (window.addEventListener) {
  window.addEventListener("message", (event) => {
    if (typeof(event.data) === "string") {
      data = JSON.parse(event.data);
    } else data = event.data
    console.log(data);
    if (data.msg) recordClick(data.msg);
  }, false);
}
else if (window.attachEvent) {  // Cross compatibility for old versions of IE
  window.attachEvent("onmessage", (event) => {
    if (typeof(event.data) === "string") {
      data = JSON.parse(event.data);
    } else data = event.data
    console.log(data);
    if (data.msg) recordClick(data.msg);
  });
}

function showHideInstructions() {
  if (document.getElementById("instructionsWrapper").classList.contains("in")){
    for (let ele of document.getElementsByClassName("collapse")){
      if (ele.classList.contains("in")) ele.classList.remove("in");
    }
  }
  else {
    for (let ele of document.getElementsByClassName("collapse")){
      if (ele.classList.contains("in")) continue;
      else ele.classList.add("in");
    }
  }
  recordClick("toggle-instructions");
}

var submit_btn = document.getElementsByClassName("btn-default")[0];
submit_btn.classList.add("hidden");  // Hide the submit button to start

function checkSubmitDisplay() {
  //Only display the submit button if the worker has interacted with the dashboard and completed the survey
  if (ratingComplete && feedbackGiven && blockMarked && annotationFinished) {
    submit_btn.classList.remove("hidden");
    window.scrollTo(0,document.body.scrollHeight);
  }
}

function usabilityRated() {
  ratingComplete = true;
  recordClick("usability-rated");
  checkSubmitDisplay();
}

function giveFeedback() {
  feedbackGiven = true;
  recordClick("feedback");
  checkSubmitDisplay();
}