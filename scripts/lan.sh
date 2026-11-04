if [ -z "$1" ]; then
    echo "Usage: source scripts/lan.sh <other laptop IP>"
    return 1 2>/dev/null || exit 1
fi

OTHER_IP="$1"
MY_IP=$(ip route get "$OTHER_IP" 2>/dev/null | sed -n 's/.* src \([0-9.]*\).*/\1/p')
if [ -z "$MY_IP" ]; then
    MY_IP=$(hostname -I | awk '{print $1}')
fi

export CYCLONEDDS_URI="<CycloneDDS><Domain id=\"any\"><General><Interfaces><NetworkInterface address=\"$MY_IP\"/></Interfaces><AllowMulticast>false</AllowMulticast></General><Discovery><ParticipantIndex>auto</ParticipantIndex><MaxAutoParticipantIndex>20</MaxAutoParticipantIndex><Peers><Peer address=\"$MY_IP\"/><Peer address=\"$OTHER_IP\"/></Peers></Discovery></Domain></CycloneDDS>"

echo "DDS LAN mode: this laptop = $MY_IP, other laptop = $OTHER_IP"
echo "Run this in EVERY terminal (server and clients) before starting the program."
