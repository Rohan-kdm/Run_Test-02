// Zoho Catalyst Advanced I/O Function (Node.js)
const express = require('express');
const app = express();
const nodemailer = require('nodemailer');

const transporter = nodemailer.createTransport({
    host: 'smtp.zoho.in', 
    port: 587, 
    secure: false, 
    auth: {
        user: "developer@opruss.com", 
        pass: "21August@2026"  
    },
    tls: {
        rejectUnauthorized: false
    }
});

app.post('/api/send-alert', express.json(), async (req, res) => {
    const { recipient, subject, body } = req.body;
    if (!recipient) return res.status(400).json({ error: "Missing recipient" });

    try {
        await transporter.sendMail({
            from: '"OPRUSS Alerts" <developer@opruss.com>',
            to: recipient,
            subject: subject || "OPRUSS Environmental Alert",
            html: `<p>${body}</p>`
        });
        res.status(200).json({ status: "success" });
    } catch (error) {
        res.status(500).json({ status: "error", message: error.toString() });
    }
});

app.post('/api/send-report', express.raw({ type: 'text/csv', limit: '5mb' }), async (req, res) => {
    const recipient = req.headers['x-recipient-email'] || req.query.recipient;
    const filename = req.headers['x-file-name'] || req.query.filename || "OPRUSS_Report.csv";
    const csvBuffer = req.body; 

    try {
        await transporter.sendMail({
            from: '"OPRUSS Reports" <developer@opruss.com>',
            to: recipient,
            subject: "Your Requested Air Quality Report",
            text: "Attached is the requested air quality monitoring report.",
            attachments: [{ filename: filename, content: csvBuffer, contentType: 'text/csv' }]
        });
        res.status(200).send("Report dispatched via Zoho SMTP");
    } catch (error) {
        res.status(500).send("Error sending report");
    }
});

module.exports = app;