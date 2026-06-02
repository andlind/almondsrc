import io
import os
import json
import pyotp
import qrcode
import socket
import stat
import logging
from werkzeug.security import check_password_hash
from cryptography.fernet import Fernet
from flask import Blueprint, render_template_string, request, redirect, url_for, session, send_file
from flask import current_app, render_template
from venv import logger

auth_blueprint = Blueprint('auth', __name__)

logon_img = '/static/almond.png'
admin_user_file = '/etc/almond/users.conf'
auth_user_file = '/etc/almond/auth2fa.enc'
is_container = 'false'
logging_on = False
user_secrets = {}

def parse_line(line):
    line = line.strip()

    # Skip empty lines or comments
    if not line or line.startswith("#"):
        return None, None

    if "=" not in line:
        return None, None

    key, value = line.split("=", 1)
    return key.strip(), value.strip()

def init_logging():
     logging.basicConfig(filename='/var/log/almond/howru.log', filemode='a', format='%(asctime)s | %(levelname)s: %(message)s', datefmt='%Y-%m-%d %H:%M:%S')
     logger = logging.getLogger()
     logger.setLevel(logging.DEBUG)
     logger_on = True

def verify_password(username, password):
    global admin_user_file, users
    users = {}
    if os.path.isfile(admin_user_file):
        with open(admin_user_file, 'r') as f:
            for line_num, line in enumerate(f, 1):
                #print ("DEBUG: ", line.strip())
                try:
                    user_data = json.loads(line.strip())
                    for user_key, hash_value in user_data.items():
                        users[user_key] = hash_value
                except json.JSONDecodeError as e:
                    print(f"Warning: Invalid JSON format at line {line_num}: {str(e)}")
                    continue
    else:
        users = {}
    if username in users:
        return check_password_hash(users.get(username), password)
    return False

def load_key():
    key = os.getenv("HOWRU_FERNET_KEY")
    if key is None:
        #raise Exception("HOWRU_FERNET_KEY environment variable not set!")
        key = Fernet.generate_key()
        logger.info("Auth2fa: Generated new fernet key")
        os.environ["HOWRU_FERNET_KEY"] = key.decode()
        print(os.getenv("HOWRU_FERNET_KEY"))
    return key.encode() if isinstance(key, str) else key

def encrypt_data(data: bytes, fernet: Fernet) -> bytes:
    return fernet.encrypt(data)

def decrypt_data(token: bytes, fernet: Fernet) -> bytes:
    return fernet.decrypt(token)

def load_user_secret(username: str, filepath: str = "/etc/almond/auth2fa.enc") -> str:
    key = load_key()
    fernet = Fernet(key)

    if not os.path.exists(filepath):
        return None

    with open(filepath, "rb") as file:
        encrypted_data = file.read()
        if not encrypted_data:
            return None
        try:
            decrypted_data = decrypt_data(encrypted_data, fernet)
            secrets = json.loads(decrypted_data.decode("utf-8"))
        except Exception as e:
            print("Error decrypting file:", e)
            print("Possible causes: incorrect key, corrupted file, or bad encryption.")
            print(os.getenv("HOWRU_FERNET_KEY"))
            return None
        
    return secrets.get(username)

def save_user_secret(username: str, secret: str, filepath: str = '/etc/almond/auth2fa.enc'):
    global user_secrets
    key = load_key()
    fernet = Fernet(key)
    if (os.path.exists(filepath)):
        with open(filepath, "rb") as file:
            encrypted_data = file.read()
            if encrypted_data:
                try:
                    decrypted_data = decrypt_data(encrypted_data, fernet)
                    user_secrets = json.loads(decrypted_data.decode("utf-8"))
                except Exception as e:
                    logger.warning("Warning: Could not decrypt the file. Starting fresh. Error: %s", e)
    user_secrets[username] = secret
    data_json = json.dumps(user_secrets)
    encrypted = encrypt_data(data_json.encode("utf-8"), fernet)
    with open(filepath, "wb") as file:
        file.write(encrypted)
    os.chmod(filepath, stat.S_IRUSR | stat.S_IWUSR)
    logger.info("Auth2fa: Saved secret for user: {username}")
        
def generate_qr_code(data):
    qr = qrcode.QRCode(
        version=1,
        error_correction=qrcode.constants.ERROR_CORRECT_H,  # High error correction
        box_size=10,  # Increase box_size to create a larger image
        border=4,
    )
    qr.add_data(data)
    qr.make(fit=True)
    img = qr.make_image(fill='black', back='white')
    return img

@auth_blueprint.route('/almond/admin/enable_2fa/<username>')
def enable_2fa(username):
    global issuer
    # Generate a new TOTP secret for the user
    logger.info("Auth2fa: Generating a new TOTP secret for user '" + username + "'")
    user_secret = pyotp.random_base32()
    user_secrets[username] = user_secret
    #print("User_secret:", user_secret)
    #print("Debug - Secret key for {}: {}".format(username, user_secret))

    # Create the provisioning URI for the authenticator app
    totp = pyotp.TOTP(user_secret)
    issuer = "howru-"
    is_container = current_app.config['IS_CONTAINER']
    if (is_container == 'true'):
        config = {}
        with open("/etc/almond/almond.conf", "r") as conf:
            for line in conf:
                key, value = parse_line(line)
                config[key] = value
        issuer = issuer + config.get('scheduler.hostName', socket.gethostname())
    else:
        issuer = issuer + socket.gethostname()
    if current_app.config['AUTH2FA_P'] == 'true':
        save_user_secret(username, user_secret, auth_user_file)
        logger.info("Auth2fa: Saving user key to encrypted file.")
    logger.info("Auth2fa: 2FA is enabled for user '" + username + "'. Auth2fa issuer is set to be '" + issuer + "'.")
    provisioning_uri = totp.provisioning_uri(name=username, issuer_name=issuer)

    # Render a simple HTML page with the QR code image embedded
    a_auth_type = current_app.config['AUTH_TYPE']
#    html = '''
#        <h1>Enable Two-Factor Authentication for {{ username }}</h1>
#        <p>Scan this QR code with your authenticator app:</p>
#        <img src="{{ url_for('auth.qr_code', username=username, issuer=issuer) }}" alt="QR Code">
#        <hr>
#        <h2>Manual Entry</h2>
#        <p>If you cannot scan the QR code, enter these details into your authenticator app:</p>
#        <ul>
#          <li><strong>Issuer:</strong> howru</li>
#          <li><strong>Account Name:</strong> {{ username }}</li>
#          <li><strong>Secret Key:</strong> {{ user_secret }}</li>
#          <li><strong>Algorithm:</strong> SHA1</li>
#          <li><strong>Digits:</strong> 6</li>
#          <li><strong>Period:</strong> 30 seconds</li>
#        </ul>
#    '''
    if (a_auth_type == "2fa"):
        #return render_template_string(html, username=username, user_secret=user_secret)
        logger.info("Auth2fa: Rendering template enablefa.html")
        return render_template('enablefa.html', logon_image=logon_img, username=username, user_secret=user_secret, issuer=issuer)
    else:
        logger.warning("Auth2fa: Auth2fa is not enabled, yet someone tried to enable user '" + username + "'.")
        return render_template("403_fa.html")    

@auth_blueprint.route('/almond/admin/qr_code/<username>')
def qr_code(username):
    global issuer
    a_auth_type = current_app.config['AUTH_TYPE']
    if (a_auth_type != "2fa"):
        logger.warning("Auth2fa: Auth2fa is not enabled, yet someone tried to look for qr_code for user '" + username + "'.")
        return render_template("403_fa.html")
    user_secret = user_secrets.get(username)
    if not user_secret:
        logger.warning("Auth2fa: Auth2fa qr_code for user '" + username + "' not found or not enabled.")
        return "User not found or 2FA not enabled.", 404
    
    totp = pyotp.TOTP(user_secret)
    provisioning_uri = totp.provisioning_uri(name=username, issuer_name=issuer)
    #qr = qrcode.make(provisioning_uri)
    qr = generate_qr_code(provisioning_uri)
    
    buf = io.BytesIO()
    qr.save(buf, format='PNG')
    buf.seek(0)
    logger.info("Auth2fa: qr_code for user '" + username + "' has been generated.")
    return send_file(buf, mimetype='image/png')

@auth_blueprint.route('/almond/admin/login', methods=['GET', 'POST'])
def login():
    if request.method == 'POST':
        username = request.form.get("uname")
        password = request.form.get("psw")
       
        #print("DEBUG username/password = %s %s" % (username, password))  
        if verify_password(username, password):
            session['username'] = username
            # Redirect to 2FA verification step
            logger.info("Auth2fa: redirecting '" + username + "' to verify_2fa")
            return redirect(url_for('auth.verify_2fa'))
        else:
            logger.warning("Auth2fa: login failed for '" + username + "'") 
            return "Invalid username or password", 401
    logging.info("Rendering template login_fa.html")
    return render_template('login_fa.html', logon_image=logon_img)

@auth_blueprint.route('/verify_2fa', methods=['GET', 'POST'])
def verify_2fa():
    username = session.get('username')
    if not username:
        logger.info("Auth2fa: No user found in session. Rederecting to auth.login")
        return redirect(url_for('auth.login'))

    if request.method == 'POST':
        token = request.form.get('token')
        if current_app.config['AUTH2FA_P'] == 'false':
            user_secret = user_secrets.get(username)
        else:
            user_secret = load_user_secret(username, auth_user_file)
        if user_secret:
            totp = pyotp.TOTP(user_secret)
            if totp.verify(token):
                session['authenticated'] = True
                session['login'] = 'true'
                session['user'] = username
                #return f"Welcome, {username}! You are fully logged in."
                logging.info("Auth2fa logged in user '" + username + "'.")
                return redirect('/almond/admin')                
            else:
                logging.warning("Auth2fa: Invalid 2FA token from user '" + username + "'.")
                return "Invalid 2FA token.", 401
        else:
            logging.warning("Auth2fa: 2FA is not enabled for user '" + username + "'.")
            return "2FA is not enabled for this account.", 400

    #return '''
    #    <h1>Two-Factor Authentication</h1>
    #    <form method="post">
    #        Enter your 2FA token: <input name="token" type="text"><br>
    #        <input type="submit" value="Verify">
    #    </form>
    #'''
    logging.info("Rendering template verify.html")
    return render_template('verify.html', logon_image=logon_img, username=username)

@auth_blueprint.route('/protected')
def protected():
    if not session.get('authenticated'):
        logging.warning("Auth2fa: No session found for authenticated. Redirecting to auth.login")
        return redirect(url_for('auth.login'))
    return "This is a protected page accessible only to fully authenticated users."
