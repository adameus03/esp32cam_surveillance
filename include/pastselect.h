R"(<!DOCTYPE html>
<html lang='pl'>
    <head>
        <meta charset='utf-8'/>
        <title>Monitoring Wybór czasu</title>
    </head>
    <body>
        <style>
            label {
                display: block;
                font: 1rem "Fira Sans", sans-serif;
            }

            input, label {
                margin: 0.4rem 0;
            }
        </style>
        <label for="playback-start-time">Wybierz początek odtwarzania:</label>
        <input type="datetime-local" id="playback-start-time" name="playback-start-time" value="2018-06-12T19:30" min="2018-06-07T00:00" max="2018-06-14T00:00" />
        <button id='play_btn'>Odtwarzaj</button>

        <script>
            window.onload=()=>{
                // Get the current datetime in the format "YYYY-MM-DDTHH:MM:SS"
                let now = new Date();
                let nowString = `${now.getFullYear()}-${(now.getMonth()+1).toString().padStart(2, '0')}-${now.getDate().toString().padStart(2, '0')}T${now.getHours().toString().padStart(2, '0')}:${now.getMinutes().toString().padStart(2, '0')}:${now.getSeconds().toString().padStart(2, '0')}`;
                // Set the current datetime as the default value for the input
                document.getElementById('playback-start-time').value = nowString;
                document.getElementById('play_btn').addEventListener('click', (e)=>{
                    let selectedDatetime = new Date(document.getElementById('playback-start-time').value);
                    // Format the date into the "YYYY-MM-DD_HH-MM-SS" format
                    let formattedDatetime = `${selectedDatetime.getFullYear()}-${(selectedDatetime.getMonth() + 1).toString().padStart(2, '0')}-${selectedDatetime.getDate().toString().padStart(2, '0')}_${selectedDatetime.getHours().toString().padStart(2, '0')}-${selectedDatetime.getMinutes().toString().padStart(2, '0')}-${selectedDatetime.getSeconds().toString().padStart(2, '0')}`;
                    document.location = `past?time=${formattedDatetime}`;
                });
            };
        </script>
    </body>
</html>)"