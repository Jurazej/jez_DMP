const gateway = `ws://${window.location.hostname}/ws`;
let websocket;
let generated = false;
window.addEventListener("load", onLoad);
function initWebSocket() {
    console.log("Trying to open a WebSocket connection...");
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}
function onOpen(event) {
    console.log("Connection opened");
    websocket.send(JSON.stringify({
        "type": "status_sync"
    }));
}
function onClose(event) {
    console.log("Connection closed");
    setTimeout(initWebSocket, 2000);
}
function onMessage(event) {
    console.log(event.data);
    const rmsg = JSON.parse(event.data);
    if (rmsg.type == "animate") {
        document.getElementById(`aSelect${rmsg.id}_${rmsg.value.part}`).selectedIndex = rmsg.value.type;
        document.getElementById(`aRange${rmsg.id}_${rmsg.value.part}_1`).value = rmsg.value.brightness;
        document.getElementById(`aRange${rmsg.id}_${rmsg.value.part}_2`).value = rmsg.value.speed;
    }
    else if (rmsg.type == "status_sync") {
        if (generated == false) {
            generateControls(rmsg.params);
        }
        for (let i = 1; i <= rmsg.nol; i++) {
            for (let j = 1; j <= rmsg.lights[i-1].nop; j++) {
                document.getElementById(`aSelect${i-1}_${j-1}`).selectedIndex = rmsg.lights[i-1].parts[j-1][0];
                document.getElementById(`aRange${i-1}_${j-1}_1`).value = rmsg.lights[i-1].parts[j-1][1];
                document.getElementById(`aRange${i-1}_${j-1}_2`).value = rmsg.lights[i-1].parts[j-1][2];    
            } 
        }
    }
}
function onLoad(event) {
    initWebSocket();
}

function animButton(i,j) {
    const params = {};
    params.part = j;
    params.brightness = Number(document.getElementById(`aRange${i}_${j}_1`).value);
    params.speed = Number(document.getElementById(`aRange${i}_${j}_2`).value);
    params.type = Number(document.getElementById(`aSelect${i}_${j}`).selectedIndex);
    wSend("animate",i,params);
}

function wSend(type,id,value=1) {
    websocket.send(JSON.stringify({
        "type": type,
        "id": Number(id),
        "value": value
    }));
}

function navMenu(id,nol) {
    if (nol == 2) {
        if (id == 1) {
            document.getElementById("nav1").style.backgroundColor = "#100e91";
            document.getElementById("nav2").style.backgroundColor = "#2a3fc9";
            document.getElementById("lightContainer1").style.display = "block";
            document.getElementById("lightContainer2").style.display = "none";
        }
        else if (id == 2) {
            document.getElementById("nav2").style.backgroundColor = "#100e91";
            document.getElementById("nav1").style.backgroundColor = "#2a3fc9";
            document.getElementById("lightContainer1").style.display = "none";
            document.getElementById("lightContainer2").style.display = "block";
        }
    }
    if (nol == 3) {
        if (id == 1) {
            document.getElementById("nav1").style.backgroundColor = "#100e91";
            document.getElementById("nav2").style.backgroundColor = "#2a3fc9";
            document.getElementById("nav3").style.backgroundColor = "#2a3fc9";
            document.getElementById("lightContainer1").style.display = "block";
            document.getElementById("lightContainer2").style.display = "none";
            document.getElementById("lightContainer3").style.display = "none";
        }
        else if (id == 2) {
            document.getElementById("nav1").style.backgroundColor = "#2a3fc9";
            document.getElementById("nav2").style.backgroundColor = "#100e91";
            document.getElementById("nav3").style.backgroundColor = "#2a3fc9";
            document.getElementById("lightContainer1").style.display = "none";
            document.getElementById("lightContainer2").style.display = "block";
            document.getElementById("lightContainer3").style.display = "none";
        }
        else if (id == 3) {
            document.getElementById("nav1").style.backgroundColor = "#2a3fc9";
            document.getElementById("nav2").style.backgroundColor = "#2a3fc9";
            document.getElementById("nav3").style.backgroundColor = "#100e91";
            document.getElementById("lightContainer1").style.display = "none";
            document.getElementById("lightContainer2").style.display = "none";
            document.getElementById("lightContainer3").style.display = "block";
        }
    }
    if (nol == 4) {
        if (id == 1) {
            document.getElementById("nav1").style.backgroundColor = "#100e91";
            document.getElementById("nav2").style.backgroundColor = "#2a3fc9";
            document.getElementById("nav3").style.backgroundColor = "#2a3fc9";
            document.getElementById("nav4").style.backgroundColor = "#2a3fc9";
            document.getElementById("lightContainer1").style.display = "block";
            document.getElementById("lightContainer2").style.display = "none";
            document.getElementById("lightContainer3").style.display = "none";
            document.getElementById("lightContainer4").style.display = "none";
        }
        else if (id == 2) {
            document.getElementById("nav1").style.backgroundColor = "#2a3fc9";
            document.getElementById("nav2").style.backgroundColor = "#100e91";
            document.getElementById("nav3").style.backgroundColor = "#2a3fc9";
            document.getElementById("nav4").style.backgroundColor = "#2a3fc9";
            document.getElementById("lightContainer1").style.display = "none";
            document.getElementById("lightContainer2").style.display = "block";
            document.getElementById("lightContainer3").style.display = "none";
            document.getElementById("lightContainer4").style.display = "none";
        }
        else if (id == 3) {
            document.getElementById("nav1").style.backgroundColor = "#2a3fc9";
            document.getElementById("nav2").style.backgroundColor = "#2a3fc9";
            document.getElementById("nav3").style.backgroundColor = "#100e91";
            document.getElementById("nav4").style.backgroundColor = "#2a3fc9";
            document.getElementById("lightContainer1").style.display = "none";
            document.getElementById("lightContainer2").style.display = "none";
            document.getElementById("lightContainer3").style.display = "block";
            document.getElementById("lightContainer4").style.display = "none";
        }
        else if (id == 4) {
            document.getElementById("nav1").style.backgroundColor = "#2a3fc9";
            document.getElementById("nav2").style.backgroundColor = "#2a3fc9";
            document.getElementById("nav3").style.backgroundColor = "#2a3fc9";
            document.getElementById("nav4").style.backgroundColor = "#100e91";
            document.getElementById("lightContainer1").style.display = "none";
            document.getElementById("lightContainer2").style.display = "none";
            document.getElementById("lightContainer3").style.display = "none";
            document.getElementById("lightContainer4").style.display = "block";
        }
    }
}

function generateControls(input) {
    generated = true;
    const lightCount = input.nol;
    if (lightCount != 1) {
        document.getElementById("menuContainer").style.display = "block";
        const menuContainer = document.getElementById("menuContainer");
        for (let i = 1; i <= lightCount; i++) {
            const menuButton = document.createElement("li");
            menuButton.style.width = `${100/lightCount}%`;
            menuButton.innerHTML =`
            <button id="nav${i}" onclick="navMenu(${i}, ${lightCount})">${i}</button>
            `;
            menuContainer.appendChild(menuButton);
    }
    navMenu(1,lightCount);
    }
    for (let i = 1; i <= lightCount; i++) {
        const lightContainer = document.getElementById(`lightContainer${i}`);
        for (let j = 1; j <= input.lights[i-1].nop; j++) {
            lightContainer.innerHTML += `
            <div class="main">
                <div class="card">
                    <h2>Ovládání části ${input.lights[i-1].part_name[j-1]}</h2>
                    <select class="aSelect" id="aSelect${i-1}_${j-1}">
                        <option>Static</option>
                        <option>Slide point right</option>
                        <option>Slide point left</option>
                        <option>Slide right</option>
                        <option>Slide left</option>
                        <option>Bouncing ball</option>
                        <option>Fill right</option>
                        <option>Fill left</option>
                    </select>
                    <div class="slideContainer">
                        <label class="rangeLabel" for="aRange${i-1}_${j-1}_1">Svítivost:</label>
                        <input type="range" min="1" max="250" value="125" id="aRange${i-1}_${j-1}_1">
                    </div>
                    <div class="slideContainer">
                        <label class="rangeLabel" for="aRange${i-1}_${j-1}_2">Rychlost:</label>
                        <input class="speed" type="range" min="200" max="10200" value="5200" id="aRange${i-1}_${j-1}_2">
                    </div>
                    <p><button onclick="animButton(${i-1},${j-1})">Odeslat</button></p>
                </div>
            </div>
            `;
        }
    }
} 