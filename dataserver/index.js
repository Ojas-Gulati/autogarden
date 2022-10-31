const express = require('express')
const app = express()
const port = 8387

const password = "ad9uwxcCkeTBSekNnrCMjj5m";

app.use(express.json());       // to support JSON-encoded bodies
app.use(express.urlencoded()); // to support URL-encoded bodies

var MongoClient = require('mongodb').MongoClient;
var url = "mongodb://localhost:27017/";

MongoClient.connect(url, function (err, db) {
    var dbo = db.db("autogarden");

    app.get('/', (req, res) => {
        res.send('OK');
    })

    app.post('/givedata', (req, res) => {
        // step 1: check password
        if (req.body.password !== password) {
            res.send('BADPASSWORD')
        }
        else {
            // we need to store the three sensors and a boardname
            var myobj = {
                boardname: req.body.boardname,
                moisture: parseInt(req.body.moisture),
                sunlight: parseInt(req.body.sunlight),
                temperature: parseInt(req.body.temperature),
                timestamp: new Date()
            };
            dbo.collection("sensordata").insertOne(myobj, function (err, result) {
                if (err) throw err;
                console.log("Recorded sensordata");
                console.log(myobj);
                res.send('OK')
            });
        }
    });

    app.post('/givewateringdata', (req, res) => {
        // step 1: check password
        if (req.body.password !== password) {
            res.send('BADPASSWORD')
        }
        else {
            // we need to store the three sensors and a boardname
            var myobj = {
                boardname: req.body.boardname,
                wateringstate: req.body.wateringstate,
                reason: req.body.reason,
                timestamp: new Date()
            };
            dbo.collection("watertimes").insertOne(myobj, function (err, result) {
                if (err) throw err;
                console.log("Recorded sensordata");
                console.log(myobj);
                res.send('OK')
            });
        }
    });
    
    app.post('/getrecords', (req, res) => {
        if (req.body.password !== password) {
            res.send('BADPASSWORD')
        }
        else {
            dbo.collection("sensordata").find({}).toArray(function(err, sensorresult) {
                if (err) throw err;
                console.log(sensorresult);
                dbo.collection("watertimes").find({}).toArray(function(err, waterresult) {
                    if (err) throw err;
                    console.log(waterresult);
                    res.send(JSON.stringify({
                        waterings: waterresult,
                        readings: sensorresult
                    }))
                });
            });
        }
    })

    app.listen(port, () => {
        console.log(`Example app listening at http://localhost:${port}`)
    })
});