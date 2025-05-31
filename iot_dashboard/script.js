const settingsForm = document.getElementById('settings-form');
const dataRangeInput = document.getElementById('data-range');
const rangeValue = document.getElementById('range-value');
const temperatureGraph = document.getElementById('temperatureGraph').getContext('2d');
let graphData = [];
let chart = null;

// ✅ Use HTTPS & no markdown formatting
const apiUrl = prompt("Enter your API URL:", "https://b728-208-131-174-130.ngrok-free.app");

// Update graph based on data range
dataRangeInput.addEventListener('input', (e) => {
    const range = e.target.value;
    rangeValue.textContent = `0-${range}`;
    updateGraph(parseInt(range));
});

// Fetch data from the server
async function fetchData(size) {
    try {
        const response = await fetch(`${apiUrl}/graph?size=${size}`);
        const data = await response.json();
        graphData = data;
        updateGraph(size);
    } catch (error) {
        console.error("Error fetching data:", error);
    }
}

// Update graph without recreating the chart
function updateGraph(size) {
    const tempData = graphData.slice(-size).map((entry) => entry.temperature);
    const timeData = graphData.slice(-size).map((entry) =>
        new Date(entry.datetime).toLocaleTimeString()
    );

    if (chart) {
        chart.data.labels = timeData;
        chart.data.datasets[0].data = tempData;
        chart.update();
    } else {
        chart = new Chart(temperatureGraph, {
            type: 'line',
            data: {
                labels: timeData,
                datasets: [{
                    label: 'Temperature (°C)',
                    data: tempData,
                    borderColor: 'rgba(75, 192, 192, 1)',
                    fill: false
                }]
            },
            options: {
                scales: {
                    x: {
                        title: {
                            display: true,
                            text: 'Time'
                        }
                    },
                    y: {
                        title: {
                            display: true,
                            text: 'Temperature (°C)'
                        }
                    }
                }
            }
        });
    }
}

// Handle form submission to update settings
settingsForm.addEventListener('submit', async (e) => {
    e.preventDefault();

    const userTemp = document.getElementById('user_temp').value;
    const userLight = document.getElementById('user_light').value;
    const lightDuration = document.getElementById('light_duration').value;

    const settings = {
        user_temp: userTemp,
        user_light: userLight,
        light_duration: lightDuration
    };

    try {
        const response = await fetch(`${apiUrl}/settings`, {
            method: 'PUT',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(settings)
        });
        const result = await response.json();
        alert('Settings updated successfully!');
    } catch (error) {
        console.error('Error updating settings:', error);
    }
});

// Initial fetch to display graph
fetchData(10);
