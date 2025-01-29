$(document).ready((()=>{let d=``,e=0,g=`m`,b=`.`,a=1,f=`e`,c=parseInt;
$.getJSON(`https://api.github.com/repos/yash-it/JbdBMS2MQTT/releases/latest`,(()=>{})).done((h=>{console.log(`get data from github done success`);
$(`#fwdownload`).attr(`href`,h.html_url);
$(`#gitversion`).text(h.tag_name.substring(a));
let j=h.tag_name.substring(a).split(b).map(a=>c(a));
let k=`%swVersion%`.split(b).map(a=>c(a));
let l=d;
for(i=e;i<j.length;i++){
 if(j[i]===k[i]){l+=f}else if(j[i]>k[i]){l+=g}else{l+=`l`}
};
if(!l.match(/[l|m]/g)){console.log(`Git-Version equal, nothing to do.`)}
else if(l.split(f).join(d)[e]==g){
 console.log(`Git-Version higher,activate notification.);
 document.getElementById(`update_alert`).style.display=d}
else{console.log(`Git-Version lower, nothing to do.`)}})).fail((()=>{console.log(`error can not get version`)}))}))

