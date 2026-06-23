function q(selector){return document.querySelector(selector);}
const video=q("#camera");
const canvas=q("#overlay");
const statusEl=q("#status");
const predictionLabel=q("#predictionLabel");
const confidenceEl=q("#confidence");
const rawGestureEl=q("#rawGesture");
const mappedSignEl=q("#mappedSign");
const handednessEl=q("#handedness");
const fingerStateEl=q("#fingerState");
const meterFill=q("#meterFill");
const transcriptEl=q("#transcript");
const CDN="https://cdn.jsdelivr.net/npm/@mediapipe/hands@0.4";
let stableSign="Waiting";
let sameSignFrames=0;
let lastTranscriptSign="";
let transcript=[];
function clamp(value,min,max){
  if(min===undefined)min=0;
  if(max===undefined)max=1;
  return Math.max(min,Math.min(max,value));
}
function distance(a,b){return Math.hypot(a.x-b.x,a.y-b.y);}function setStatus(message,tone){
  statusEl.textContent=message;
  if(tone==="warn")statusEl.style.color="var(--warn)";
  else statusEl.style.color="var(--muted)";
}
function resizeCanvas(){
  const rect=video.getBoundingClientRect();
  canvas.width=Math.max(1,Math.round(rect.width));
  canvas.height=Math.max(1,Math.round(rect.height));
}
function correctedHandedness(label){
  if(label==="Left")return "Right physical hand";
  if(label==="Right")return "Left physical hand";
  if(label)return label;
  return "-";
}
function fingerExtension(landmarks,tip,pip){
  const yScore=clamp((landmarks[pip].y-landmarks[tip].y+0.03)/0.20);
  const wristScore=clamp((distance(landmarks[tip],landmarks[0])-distance(landmarks[pip],landmarks[0])+0.02)/0.12);
  return clamp(yScore*0.8+wristScore*0.2);
}
function thumbExtension(landmarks){
  const palmScale=Math.max(0.001,distance(landmarks[0],landmarks[9]));
  const sideScore=clamp((distance(landmarks[4],landmarks[5])/palmScale-0.55)/0.50);
  const upScore=clamp((landmarks[3].y-landmarks[4].y+0.02)/0.18);
  return Math.max(sideScore,upScore);
}
function average(values){
  let sum=0;
  for(let i=0;i<values.length;i++)sum+=values[i];
  return sum/values.length;
}function fingerWord(name,value){
  return name+":"+(value>=0.55?"up":"down");
}
function classifyLandmarks(landmarks){
  const thumb=thumbExtension(landmarks);
  const index=fingerExtension(landmarks,8,6);
  const middle=fingerExtension(landmarks,12,10);
  const ring=fingerExtension(landmarks,16,14);
  const pinky=fingerExtension(landmarks,20,18);
  const fingerState=[
    fingerWord("T",thumb),
    fingerWord("I",index),
    fingerWord("M",middle),
    fingerWord("R",ring),
    fingerWord("P",pinky)
  ].join(" ");
  const ilyScore=average([thumb,index,1-middle,1-ring,pinky]);
  let looksLikeIly=true;
  if(thumb<0.45)looksLikeIly=false;
  if(index<0.55)looksLikeIly=false;
  if(pinky<0.55)looksLikeIly=false;
  if(middle>0.52)looksLikeIly=false;
  if(ring>0.52)looksLikeIly=false;
  if(looksLikeIly){
    return {raw:"I-L-Y handshape",sign:"I Love You",score:ilyScore,fingerState:fingerState};
  }
  const candidates=[
    {raw:"Open palm",sign:"Hello",score:average([thumb*0.75,index,middle,ring,pinky])},
    {raw:"Closed fist",sign:"Yes",score:average([1-index,1-middle,1-ring,1-pinky,1-thumb*0.4])},
    {raw:"Victory",sign:"No",score:average([index,middle,1-ring,1-pinky])},
    {raw:"Thumb up",sign:"Yes",score:average([thumb,1-index,1-middle,1-ring,1-pinky])},
    {raw:"I-L-Y handshape",sign:"I Love You",score:ilyScore}
  ];
  candidates.sort(function(a,b){return b.score-a.score;});
  candidates[0].fingerState=fingerState;
  if(candidates[0].score<0.42){
    return {raw:"Unclear",sign:"Unknown",score:candidates[0].score,fingerState:fingerState};
  }
  return candidates[0];
}function updatePrediction(sign,rawGesture,confidence,handedness,fingerState){
  const boundedConfidence=clamp(confidence);
  predictionLabel.textContent=sign;
  confidenceEl.textContent=Math.round(boundedConfidence*100)+"%";
  rawGestureEl.textContent=rawGesture;
  mappedSignEl.textContent=sign;
  handednessEl.textContent=handedness;
  if(fingerStateEl){
    if(fingerState)fingerStateEl.textContent=fingerState;
    else fingerStateEl.textContent="-";
  }
  meterFill.style.width=Math.round(boundedConfidence*100)+"%";
  if(sign===stableSign){sameSignFrames+=1;}else{stableSign=sign;sameSignFrames=1;}
  let shouldAppend=true;
  if(sign==="No hand detected")shouldAppend=false;
  if(sign==="Unknown")shouldAppend=false;
  if(sign==="Waiting")shouldAppend=false;
  if(10>sameSignFrames)shouldAppend=false;
  if(sign===lastTranscriptSign)shouldAppend=false;
  if(shouldAppend){
    transcript.push(sign);
    if(transcript.length>18)transcript=transcript.slice(-18);
    lastTranscriptSign=sign;
    transcriptEl.textContent=transcript.join(" / ");
  }
}function drawResults(results){
  resizeCanvas();
  const context=canvas.getContext("2d");
  context.save();
  context.clearRect(0,0,canvas.width,canvas.height);
  if(results.multiHandLandmarks){
    for(const landmarks of results.multiHandLandmarks){
      drawConnectors(context,landmarks,HAND_CONNECTIONS,{color:"#34d399",lineWidth:4});
      drawLandmarks(context,landmarks,{color:"#f4f7fb",lineWidth:1,radius:3});
    }
  }
  context.restore();
}function onResults(results){
  drawResults(results);
  let hasHand=true;
  if(!results.multiHandLandmarks)hasHand=false;
  if(hasHand){
    if(results.multiHandLandmarks.length===0)hasHand=false;
  }
  if(!hasHand){
    updatePrediction("No hand detected","None",0,"-","-");
    return;
  }
  const best=classifyLandmarks(results.multiHandLandmarks[0]);
  let rawHandedness="-";
  if(results.multiHandedness){
    if(results.multiHandedness[0])rawHandedness=results.multiHandedness[0].label;
  }
  updatePrediction(best.sign,best.raw,best.score,correctedHandedness(rawHandedness),best.fingerState);
}async function start(){
  try{
    let runtimeLoaded=true;
    if(window.Hands===undefined)runtimeLoaded=false;
    if(window.Camera===undefined)runtimeLoaded=false;
    if(runtimeLoaded===false)throw new Error("MediaPipe runtime did not load from the CDN.");
    setStatus("Loading hand tracking model...");
    const hands=new Hands({locateFile:function(file){return CDN+"/"+file;}});
    hands.setOptions({maxNumHands:1,modelComplexity:1,minDetectionConfidence:0.58,minTrackingConfidence:0.58});
    hands.onResults(onResults);
    setStatus("Requesting camera access...");
    const camera=new Camera(video,{onFrame:async function(){await hands.send({image:video});},width:1280,height:720});
    await camera.start();
    updatePrediction("Waiting","None",0,"-","-");
    setStatus("Camera live. Handedness is corrected for the mirrored selfie view.");
  }catch(error){
    console.error(error);
    setStatus("Unable to start live recognition: "+error.message,"warn");
    predictionLabel.textContent="Camera unavailable";
  }
}
start();