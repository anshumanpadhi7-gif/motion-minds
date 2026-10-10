frontend/app.js.
const $ = (id) => document.getElementById(id);

const chart = $("temperature-chart");
const ctx = chart.getContext("2d");
let readings = [];
let backendOnline = false;

function resizeChart() {
  const rect = chart.getBoundingClientRect();
  const dpr = window.devicePixelRatio || 1;

  chart.width = Math.max(1, Math.round(rect.width * dpr));
  chart.height = Math.max(1, Math.round(rect.height * dpr));
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  drawChart();
}

function drawChart() {
  const width = chart.clientWidth;
  const height = chart.clientHeight;

  ctx.clearRect(0, 0, width, height);

  const values = readings
    .map(r => Number(r.temperature_c))
    .filter(Number.isFinite);

  $("empty-chart").style.display = values.length ? "none" : "block";
  if (!values.length) return;

  const min = Math.min(...values) - 1;
  const max = Math.max(...values) + 1;
  const range = Math.max(max - min, 1);
  const left = 38, right = 12, top = 15, bottom = 28;
  const plotW = width - left - right;
  const plotH = height - top - bottom;

  ctx.font = "11px system-ui";
  ctx.strokeStyle = "#2a3a4f";
  ctx.fillStyle = "#9aaabd";
  ctx.lineWidth = 1;

  for (let i = 0; i <= 4; i++) {
    const y = top + plotH * i / 4;
    const value = max - range * i / 4;
    ctx.beginPath();
    ctx.moveTo(left, y);
    ctx.lineTo(width - right, y);
    ctx.stroke();
    ctx.fillText(value.toFixed(1), 2, y + 4);
  }

  if (values.length === 1) {
    const y = top + plotH * (max - values[0]) / range;
    ctx.fillStyle = "#64e0c0";
    ctx.beginPath();
    ctx.arc(left + plotW / 2, y, 4, 0, Math.PI * 2);
    ctx.fill();
    return;
  }

  ctx.strokeStyle = "#64e0c0";
  ctx.lineWidth = 2.5;
  ctx.beginPath();

  values.forEach((value, index) => {
    const x = left + plotW * index / (values.length - 1);
    const y = top + plotH * (max - value) / range;
    if (index === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
  });

  ctx.stroke();
}

function formatTime(timestamp) {
  if (!timestamp) return "No reading";
  const date = new Date(timestamp);
  return Number.isNaN(date.getTime()) ? "Unknown" :
    date.toLocaleTimeString();
}

async function refresh() {
  try {
    const [latestResponse, historyResponse, predictionResponse, healthResponse] =
      await Promise.all([
        fetch("/api/latest"),
        fetch("/api/history"),
        fetch("/api/prediction"),
        fetch("/api/health")
      ]);

    if (![latestResponse, historyResponse, predictionResponse, healthResponse]
      .every(r => r.ok)) {
      throw new Error("Backend request failed");
    }

    const latestData = await latestResponse.json();
    const historyData = await historyResponse.json();
    const predictionData = await predictionResponse.json();
    const healthData = await healthResponse.json();

    backendOnline = true;
    $("connection").textContent = "Backend online";
    $("connection").className = "status-pill online";
    $("backend-status").textContent = "Online";

    const current = latestData.reading;
    if (current) {
      $("temperature").textContent =
        Number(current.temperature_c).toFixed(2);
      $("humidity").textContent =
        Number(current.humidity_pct).toFixed(1);
      $("sensor-status").textContent = "Data received";
      $("source-label").textContent = current.source || "Unknown";
      $("last-reading").textContent = formatTime(current.timestamp);
      $("updated").textContent =
        "Last reading: " + formatTime(current.timestamp);
      $("data-source").textContent = current.source || "Unknown";
    } else {
      $("sensor-status").textContent = "Waiting";
      $("source-label").textContent = "No sensor data yet";
    }

    readings = (historyData.readings || []).slice(-60);
    drawChart();

    if (predictionData.available) {
      $("prediction").textContent =
        Number(predictionData.predicted_temperature_c).toFixed(2);
      $("model-label").textContent = "Baseline · 15 minutes";
      $("prediction-note").textContent =
        "This is a persistence baseline. It assumes the future " +
        "temperature equals the latest valid reading; it is not a trained AI forecast.";
    } else {
      $("prediction").textContent = "--";
      $("model-label").textContent = "Waiting for readings";
      $("prediction-note").textContent = predictionData.message;
    }

    $("backend-status").textContent =
      healthData.backend === "running" ? "Running" : "Unknown";

  } catch (error) {
    backendOnline = false;
    $("connection").textContent = "Backend unreachable";
    $("connection").className = "status-pill offline";
    $("backend-status").textContent = "Unreachable";
  }
}

window.addEventListener("resize", resizeChart);
resizeChart();
refresh();
setInterval(refresh, 3000);