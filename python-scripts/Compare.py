import requests

import requests
import json
from datetime import datetime

APP_RIS = "Ris1"
APP_STORE_SERVER = "StoreServer1"
APP_EXPORTER = "Exporter1"
APP_TRANSFER = "Transfer1"


TODAY = datetime. today().strftime('%Y%m%d')

# URL và thông tin xác thực
self_base_url = "http://192.168.1.40:8045"

self_headers = {
    "Content-Type": "application/json",
    "Authorization": "Basic b3J0aGFuYzpvcnRoYW5j"
}

remote_base_url = "http://192.168.1.40:8042"
remote_headers = {
    "Content-Type": "application/json",
    "Authorization": "Basic b3J0aGFuYzpvcnRoYW5j"
}

body = {
    "Level": "Study",
    "Expand": True,
    "Query": {"StudyDate": TODAY}
}

def GetStudies(url, headers):
    find_url = "/tools/find"
    response = requests.post(self_base_url + find_url, headers=headers, json=body, timeout=10)

    items = response.json()
    results = {}
    for item in items:
        result = {}
        result["StudyInstanceUID"] = item["MainDicomTags"]["StudyInstanceUID"]
        result["Series"] = item["Series"]

        results[item["ID"]] = {
            "StudyInstanceUID": item["MainDicomTags"]["StudyInstanceUID"],
            "AccessionNumber": item["MainDicomTags"]["AccessionNumber"],
            "Series": item["Series"]
        }

    return results

# def Diff(a_series, b_series)

def Compare(url, headers, studies):
    results = []

    for studyId in studies:
        res = requests.get(url + "/studies/" + studyId, headers=headers, timeout=None)
        if not res.ok:
            results = results + studies[studyId]["Series"]
        else:
            res_json = res.json()
            diff = list(set(studies[studyId]["Series"]) - set(res_json["Series"]))
            results = results + diff

    return results


self_studies = GetStudies(self_base_url, self_headers)
diff_series = Compare(remote_base_url, remote_headers, self_studies)
print(diff_series)

for series in diff_series:
    r_body = {
        "iuid": "1.2.3",
        "resource_id": series,
        "resource_type": "series",
        "app": APP_TRANSFER
    }
    print(r_body)
    response = requests.post(self_base_url + "/itech/execute-event-queues", headers=remote_headers, json=r_body)
    print(response.json())




# print(GetStudies(self_base_url, self_headers))

# r = requests.get(self_base_url + "/studies/760ece80-fe24ca8d-affd89b1-171bf304-1df75054", headers=self_headers, timeout=1)



# a = ["123", "456", "789"]
# b = ["123"]

# # Elements in list1 but not in list2
# c = list(set(a) - set(b))
# print(c)



