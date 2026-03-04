import requests

url = "https://ky28nlq70u99.dssldrf.net/PINGME"
try:
    # We use a timeout to ensure the script doesn't hang indefinitely
    response = requests.get(url, timeout=10)
    # Check if the request was successful (status code 200-299)
    if response.status_code == 200:
        print(f"Success! Response from server: {response.text}")
    else:
        print(f"Pinged, but received status code: {response.status_code}")
except requests.exceptions.RequestException as e:
    # This catches connection errors, timeouts, and DNS issues
    print(f"An error occurred: {e}")
